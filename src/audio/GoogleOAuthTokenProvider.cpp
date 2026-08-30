#include "audio/GoogleOAuthTokenProvider.h"

#include <windows.h>

#include <bcrypt.h>
#include <wincrypt.h>
#include <winhttp.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <ctime>
#include <format>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/Logger.h"
#include "core/StringConvert.h"

namespace sveta::audio {

namespace {

constexpr DWORD kTimeoutMs = 15000;
// Refresh a bit before real expiry (Google tokens last ~3600s) so a
// request that starts right before the cutoff doesn't get a token that
// dies mid-flight.
constexpr int kRefreshMarginSeconds = 300;
constexpr int kRequestedLifetimeSeconds = 3600;

class WinHttpHandle {
public:
    explicit WinHttpHandle(HINTERNET handle = nullptr) : handle_(handle) {}
    ~WinHttpHandle() {
        if (handle_) {
            WinHttpCloseHandle(handle_);
        }
    }
    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;

    HINTERNET Get() const { return handle_; }
    explicit operator bool() const { return handle_ != nullptr; }

private:
    HINTERNET handle_;
};

struct ParsedUrl {
    std::wstring host;
    std::wstring path;
};

std::optional<ParsedUrl> ParseUrl(const std::wstring& url) {
    wchar_t host[256]{};
    wchar_t path[2048]{};
    wchar_t extra[2048]{};

    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.lpszHostName = host;
    components.dwHostNameLength = static_cast<DWORD>(std::size(host));
    components.lpszUrlPath = path;
    components.dwUrlPathLength = static_cast<DWORD>(std::size(path));
    components.lpszExtraInfo = extra;
    components.dwExtraInfoLength = static_cast<DWORD>(std::size(extra));

    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &components)) {
        return std::nullopt;
    }

    ParsedUrl parsed;
    parsed.host = host;
    parsed.path = std::wstring(path) + extra;
    return parsed;
}

std::string Base64UrlEncode(const std::vector<BYTE>& bytes) {
    DWORD len = 0;
    CryptBinaryToStringA(bytes.data(), static_cast<DWORD>(bytes.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &len);
    std::string base64(len, '\0');
    CryptBinaryToStringA(
        bytes.data(), static_cast<DWORD>(bytes.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, base64.data(),
        &len);
    base64.resize(len);

    for (char& c : base64) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    while (!base64.empty() && base64.back() == '=') {
        base64.pop_back();
    }
    return base64;
}

std::string Base64UrlEncode(const std::string& text) {
    return Base64UrlEncode(std::vector<BYTE>(text.begin(), text.end()));
}

// Converts a legacy CAPI PRIVATEKEYBLOB (as produced by
// CryptDecodeObjectEx(PKCS_RSA_PRIVATE_KEY, ...), little-endian fields)
// into a BCRYPT_RSAFULLPRIVATE_BLOB (big-endian fields) that
// BCryptImportKeyPair understands. Field order is identical between the
// two formats; only per-field endianness (and the public exponent's
// variable trimmed length) differs. Verified against an independent
// .NET RSA.SignData signature over the same key/message during
// development -- byte-identical output.
std::optional<std::vector<BYTE>> ConvertCapiBlobToBCryptBlob(const std::vector<BYTE>& capiBlob) {
    if (capiBlob.size() < sizeof(BLOBHEADER) + sizeof(RSAPUBKEY)) {
        return std::nullopt;
    }

    const auto* header = reinterpret_cast<const BLOBHEADER*>(capiBlob.data());
    const auto* pubKey = reinterpret_cast<const RSAPUBKEY*>(capiBlob.data() + sizeof(BLOBHEADER));
    constexpr DWORD kRsa2Magic = 0x32415352; // 'RSA2' as a little-endian DWORD
    if (header->bType != PRIVATEKEYBLOB || pubKey->magic != kRsa2Magic) {
        return std::nullopt;
    }

    const DWORD bitLen = pubKey->bitlen;
    const DWORD modulusLen = bitLen / 8;
    const DWORD primeLen = bitLen / 16;
    if (capiBlob.size() < sizeof(BLOBHEADER) + sizeof(RSAPUBKEY) + modulusLen * 2 + primeLen * 4) {
        return std::nullopt;
    }

    const BYTE* cursor = capiBlob.data() + sizeof(BLOBHEADER) + sizeof(RSAPUBKEY);
    const auto take = [&cursor](DWORD len) {
        std::vector<BYTE> v(cursor, cursor + len);
        cursor += len;
        return v;
    };
    const auto toBigEndian = [](std::vector<BYTE> littleEndian) {
        std::reverse(littleEndian.begin(), littleEndian.end());
        return littleEndian;
    };

    const auto modulus = take(modulusLen);
    const auto prime1 = take(primeLen);
    const auto prime2 = take(primeLen);
    const auto exponent1 = take(primeLen);
    const auto exponent2 = take(primeLen);
    const auto coefficient = take(primeLen);
    const auto privateExponent = take(modulusLen);

    // RSAPUBKEY.pubexp is a 4-byte little-endian DWORD; BCRYPT wants the
    // minimal big-endian representation (leading zero bytes stripped).
    std::vector<BYTE> pubExpBE(
        reinterpret_cast<const BYTE*>(&pubKey->pubexp), reinterpret_cast<const BYTE*>(&pubKey->pubexp) + sizeof(DWORD));
    std::reverse(pubExpBE.begin(), pubExpBE.end());
    size_t firstNonZero = 0;
    while (firstNonZero + 1 < pubExpBE.size() && pubExpBE[firstNonZero] == 0) {
        ++firstNonZero;
    }
    pubExpBE.erase(pubExpBE.begin(), pubExpBE.begin() + firstNonZero);

    BCRYPT_RSAKEY_BLOB blobHeader{};
    blobHeader.Magic = BCRYPT_RSAFULLPRIVATE_MAGIC;
    blobHeader.BitLength = bitLen;
    blobHeader.cbPublicExp = static_cast<ULONG>(pubExpBE.size());
    blobHeader.cbModulus = modulusLen;
    blobHeader.cbPrime1 = primeLen;
    blobHeader.cbPrime2 = primeLen;

    std::vector<BYTE> out(reinterpret_cast<BYTE*>(&blobHeader), reinterpret_cast<BYTE*>(&blobHeader) + sizeof(blobHeader));
    const auto append = [&out](const std::vector<BYTE>& v) { out.insert(out.end(), v.begin(), v.end()); };
    append(pubExpBE);
    append(toBigEndian(modulus));
    append(toBigEndian(prime1));
    append(toBigEndian(prime2));
    append(toBigEndian(exponent1));
    append(toBigEndian(exponent2));
    append(toBigEndian(coefficient));
    append(toBigEndian(privateExponent));
    return out;
}

std::optional<std::vector<BYTE>> SignRs256(const std::string& privateKeyPem, const std::string& signingInput) {
    DWORD derSize = 0;
    if (!CryptStringToBinaryA(
            privateKeyPem.c_str(), static_cast<DWORD>(privateKeyPem.size()), CRYPT_STRING_BASE64HEADER, nullptr,
            &derSize, nullptr, nullptr)) {
        return std::nullopt;
    }
    std::vector<BYTE> der(derSize);
    if (!CryptStringToBinaryA(
            privateKeyPem.c_str(), static_cast<DWORD>(privateKeyPem.size()), CRYPT_STRING_BASE64HEADER, der.data(),
            &derSize, nullptr, nullptr)) {
        return std::nullopt;
    }

    LPVOID pkiPtr = nullptr;
    DWORD pkiSize = 0;
    if (!CryptDecodeObjectEx(
            X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, PKCS_PRIVATE_KEY_INFO, der.data(), derSize,
            CRYPT_DECODE_ALLOC_FLAG, nullptr, &pkiPtr, &pkiSize)) {
        core::Logger::Warn(std::format("GoogleOAuthTokenProvider: PKCS#8 decode failed (error={})", GetLastError()));
        return std::nullopt;
    }
    const auto* pki = reinterpret_cast<CRYPT_PRIVATE_KEY_INFO*>(pkiPtr);

    LPVOID blobPtr = nullptr;
    DWORD blobSize = 0;
    const bool decoded = CryptDecodeObjectEx(
        X509_ASN_ENCODING, PKCS_RSA_PRIVATE_KEY, pki->PrivateKey.pbData, pki->PrivateKey.cbData,
        CRYPT_DECODE_ALLOC_FLAG, nullptr, &blobPtr, &blobSize);
    if (!decoded) {
        core::Logger::Warn(std::format("GoogleOAuthTokenProvider: PKCS#1 decode failed (error={})", GetLastError()));
        LocalFree(pkiPtr);
        return std::nullopt;
    }
    const std::vector<BYTE> capiBlob(reinterpret_cast<BYTE*>(blobPtr), reinterpret_cast<BYTE*>(blobPtr) + blobSize);
    LocalFree(pkiPtr);
    LocalFree(blobPtr);

    auto bcryptBlob = ConvertCapiBlobToBCryptBlob(capiBlob);
    if (!bcryptBlob) {
        core::Logger::Warn("GoogleOAuthTokenProvider: unexpected RSA key blob shape");
        return std::nullopt;
    }

    BCRYPT_ALG_HANDLE algHandle = nullptr;
    if (BCryptOpenAlgorithmProvider(&algHandle, BCRYPT_RSA_ALGORITHM, nullptr, 0) != 0) {
        return std::nullopt;
    }
    BCRYPT_KEY_HANDLE keyHandle = nullptr;
    const NTSTATUS importStatus = BCryptImportKeyPair(
        algHandle, nullptr, BCRYPT_RSAFULLPRIVATE_BLOB, &keyHandle, bcryptBlob->data(),
        static_cast<ULONG>(bcryptBlob->size()), 0);
    if (importStatus != 0) {
        core::Logger::Warn(std::format("GoogleOAuthTokenProvider: BCryptImportKeyPair failed (0x{:08X})", static_cast<unsigned>(importStatus)));
        BCryptCloseAlgorithmProvider(algHandle, 0);
        return std::nullopt;
    }

    BCRYPT_ALG_HANDLE hashAlgHandle = nullptr;
    BCryptOpenAlgorithmProvider(&hashAlgHandle, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    BYTE hash[32]{};
    BCryptHash(
        hashAlgHandle, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<char*>(signingInput.data())),
        static_cast<ULONG>(signingInput.size()), hash, sizeof(hash));
    BCryptCloseAlgorithmProvider(hashAlgHandle, 0);

    BCRYPT_PKCS1_PADDING_INFO paddingInfo{};
    paddingInfo.pszAlgId = BCRYPT_SHA256_ALGORITHM;

    DWORD signatureLen = 0;
    BCryptSignHash(keyHandle, &paddingInfo, hash, sizeof(hash), nullptr, 0, &signatureLen, BCRYPT_PAD_PKCS1);
    std::vector<BYTE> signature(signatureLen);
    const NTSTATUS signStatus =
        BCryptSignHash(keyHandle, &paddingInfo, hash, sizeof(hash), signature.data(), signatureLen, &signatureLen, BCRYPT_PAD_PKCS1);

    BCryptDestroyKey(keyHandle);
    BCryptCloseAlgorithmProvider(algHandle, 0);

    if (signStatus != 0) {
        core::Logger::Warn(std::format("GoogleOAuthTokenProvider: BCryptSignHash failed (0x{:08X})", static_cast<unsigned>(signStatus)));
        return std::nullopt;
    }
    return signature;
}

std::optional<std::string> BuildSignedJwt(const GoogleServiceAccount& account) {
    const auto now = static_cast<long long>(std::time(nullptr));

    nlohmann::json header;
    header["alg"] = "RS256";
    header["typ"] = "JWT";

    nlohmann::json claims;
    claims["iss"] = account.clientEmail;
    claims["scope"] = "https://www.googleapis.com/auth/cloud-platform";
    claims["aud"] = account.tokenUri;
    claims["iat"] = now;
    claims["exp"] = now + kRequestedLifetimeSeconds;

    const std::string signingInput = Base64UrlEncode(header.dump()) + "." + Base64UrlEncode(claims.dump());
    const auto signature = SignRs256(account.privateKeyPem, signingInput);
    if (!signature) {
        return std::nullopt;
    }
    return signingInput + "." + Base64UrlEncode(*signature);
}

std::string UrlEncode(const std::string& text) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : text) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += kHex[c >> 4];
            out += kHex[c & 0xF];
        }
    }
    return out;
}

} // namespace

GoogleOAuthTokenProvider::GoogleOAuthTokenProvider(GoogleServiceAccount account) : account_(std::move(account)) {}

std::optional<std::string> GoogleOAuthTokenProvider::GetAccessToken() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!cachedToken_.empty() && std::chrono::steady_clock::now() < expiresAt_) {
            return cachedToken_;
        }
    }

    const auto token = MintAccessToken();
    if (!token) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    cachedToken_ = *token;
    expiresAt_ = std::chrono::steady_clock::now() +
        std::chrono::seconds(kRequestedLifetimeSeconds - kRefreshMarginSeconds);
    return cachedToken_;
}

std::optional<std::string> GoogleOAuthTokenProvider::MintAccessToken() const {
    if (!account_.IsUsable()) {
        return std::nullopt;
    }

    const auto jwt = BuildSignedJwt(account_);
    if (!jwt) {
        return std::nullopt;
    }

    const auto url = ParseUrl(core::Utf8ToWide(account_.tokenUri));
    if (!url) {
        return std::nullopt;
    }

    const std::string body = "grant_type=" + UrlEncode("urn:ietf:params:oauth:grant-type:jwt-bearer") +
        "&assertion=" + UrlEncode(*jwt);

    WinHttpHandle session(WinHttpOpen(
        L"SvetaAssistant/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) {
        return std::nullopt;
    }
    WinHttpSetTimeouts(session.Get(), kTimeoutMs, kTimeoutMs, kTimeoutMs, kTimeoutMs);

    WinHttpHandle connect(WinHttpConnect(session.Get(), url->host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!connect) {
        return std::nullopt;
    }

    WinHttpHandle request(WinHttpOpenRequest(
        connect.Get(), L"POST", url->path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE));
    if (!request) {
        return std::nullopt;
    }

    const std::wstring headers = L"Content-Type: application/x-www-form-urlencoded\r\n";
    if (!WinHttpSendRequest(
            request.Get(), headers.c_str(), static_cast<DWORD>(-1L), const_cast<char*>(body.data()),
            static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0)) {
        core::Logger::Warn(std::format("GoogleOAuthTokenProvider: token request failed (error={})", GetLastError()));
        return std::nullopt;
    }
    if (!WinHttpReceiveResponse(request.Get(), nullptr)) {
        return std::nullopt;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(
        request.Get(), WINHTTP_QUERY_FLAG_NUMBER | WINHTTP_QUERY_STATUS_CODE, WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);

    std::string responseBody;
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(request.Get(), &available) && available > 0) {
        std::string chunk(available, '\0');
        DWORD bytesRead = 0;
        if (!WinHttpReadData(request.Get(), chunk.data(), available, &bytesRead)) {
            break;
        }
        chunk.resize(bytesRead);
        responseBody += chunk;
    }

    if (statusCode != 200) {
        core::Logger::Warn(
            std::format("GoogleOAuthTokenProvider: token endpoint HTTP {} — {}", statusCode, responseBody.substr(0, 300)));
        return std::nullopt;
    }

    try {
        const auto parsed = nlohmann::json::parse(responseBody);
        return parsed.at("access_token").get<std::string>();
    } catch (const nlohmann::json::exception& e) {
        core::Logger::Warn(std::string("GoogleOAuthTokenProvider: failed to parse token response: ") + e.what());
        return std::nullopt;
    }
}

} // namespace sveta::audio
