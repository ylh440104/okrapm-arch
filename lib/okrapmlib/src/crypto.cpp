#include "okrapmlib/crypto.h"

#ifndef OKRAPM_WITH_CRYPTO
#define OKRAPM_WITH_CRYPTO 1
#endif

#if !OKRAPM_WITH_CRYPTO

namespace okrapm {
std::string Crypto::Base64Encode(const std::vector<unsigned char>&) { return {}; }
std::optional<std::vector<unsigned char>> Crypto::Base64Decode(const std::string&) { return std::nullopt; }
std::string Crypto::Sha256Hex(const std::vector<unsigned char>&) { return {}; }
std::string Crypto::Sha256File(const std::string&) { return {}; }
std::string Crypto::NowStamp() { return {}; }
std::optional<KeyPair> Crypto::Generate() { return std::nullopt; }
std::optional<std::string> Crypto::Sign(const std::string&, const std::vector<unsigned char>&) { return std::nullopt; }
bool Crypto::Verify(const std::string&, const std::vector<unsigned char>&, const std::string&) { return false; }
std::string Crypto::KeyIdFromPublic(const std::string&) { return {}; }
}

#else

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

namespace fs = std::filesystem;

namespace okrapm {

namespace {

const char Base64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::vector<std::string> Split(const std::string& Value, char Sep) {
    std::vector<std::string> Parts;
    std::stringstream Stream(Value);
    std::string Item;
    while (std::getline(Stream, Item, Sep)) Parts.push_back(Item);
    return Parts;
}

std::string Trim(const std::string& Value) {
    auto Begin = Value.find_first_not_of(" \t\r\n");
    if (Begin == std::string::npos) return {};
    auto End = Value.find_last_not_of(" \t\r\n");
    return Value.substr(Begin, End - Begin + 1);
}

std::vector<unsigned char> ReadAll(const std::string& Path) {
    std::ifstream Input(Path, std::ios::binary);
    if (!Input.is_open()) return {};
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(Input)),
                                     std::istreambuf_iterator<char>());
}

}

std::string Crypto::Base64Encode(const std::vector<unsigned char>& Data) {
    std::string Out;
    size_t Index = 0;
    while (Index + 2 < Data.size()) {
        unsigned int Triple = (Data[Index] << 16) | (Data[Index + 1] << 8) | Data[Index + 2];
        Out += Base64Chars[(Triple >> 18) & 0x3F];
        Out += Base64Chars[(Triple >> 12) & 0x3F];
        Out += Base64Chars[(Triple >> 6) & 0x3F];
        Out += Base64Chars[Triple & 0x3F];
        Index += 3;
    }
    size_t Remainder = Data.size() - Index;
    if (Remainder == 1) {
        unsigned int Triple = Data[Index] << 16;
        Out += Base64Chars[(Triple >> 18) & 0x3F];
        Out += Base64Chars[(Triple >> 12) & 0x3F];
        Out += "==";
    } else if (Remainder == 2) {
        unsigned int Triple = (Data[Index] << 16) | (Data[Index + 1] << 8);
        Out += Base64Chars[(Triple >> 18) & 0x3F];
        Out += Base64Chars[(Triple >> 12) & 0x3F];
        Out += Base64Chars[(Triple >> 6) & 0x3F];
        Out += "=";
    }
    return Out;
}

std::optional<std::vector<unsigned char>> Crypto::Base64Decode(const std::string& Text) {
    std::vector<int> Table(256, -1);
    for (int i = 0; i < 64; ++i) Table[static_cast<unsigned char>(Base64Chars[i])] = i;
    std::vector<unsigned char> Out;
    int Buffer = 0;
    int Bits = 0;
    for (char C : Text) {
        if (C == '=' || C == '\n' || C == '\r' || C == ' ' || C == '\t') continue;
        int Value = Table[static_cast<unsigned char>(C)];
        if (Value < 0) return std::nullopt;
        Buffer = (Buffer << 6) | Value;
        Bits += 6;
        if (Bits >= 8) {
            Bits -= 8;
            Out.push_back(static_cast<unsigned char>((Buffer >> Bits) & 0xFF));
        }
    }
    return Out;
}

std::string Crypto::Sha256Hex(const std::vector<unsigned char>& Data) {
    unsigned char Digest[SHA256_DIGEST_LENGTH];
    SHA256(Data.data(), Data.size(), Digest);
    static const char Hex[] = "0123456789abcdef";
    std::string Out;
    Out.reserve(SHA256_DIGEST_LENGTH * 2);
    for (unsigned char Byte : Digest) {
        Out += Hex[(Byte >> 4) & 0x0F];
        Out += Hex[Byte & 0x0F];
    }
    return Out;
}

std::string Crypto::Sha256File(const std::string& Path) {
    auto Data = ReadAll(Path);
    if (Data.empty() && !fs::exists(Path)) return {};
    return Sha256Hex(Data);
}

std::string Crypto::NowStamp() {
    std::time_t Now = std::time(nullptr);
    std::tm Utc{};
    gmtime_r(&Now, &Utc);
    char Buffer[32];
    std::strftime(Buffer, sizeof(Buffer), "%Y-%m-%d", &Utc);
    return Buffer;
}

std::optional<KeyPair> Crypto::Generate() {
    EVP_PKEY* Key = nullptr;
    EVP_PKEY_CTX* Ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
    if (!Ctx) return std::nullopt;
    if (EVP_PKEY_keygen_init(Ctx) <= 0 || EVP_PKEY_keygen(Ctx, &Key) <= 0) {
        EVP_PKEY_CTX_free(Ctx);
        return std::nullopt;
    }
    EVP_PKEY_CTX_free(Ctx);

    KeyPair Result;
    std::vector<unsigned char> Raw(64);
    size_t Length = Raw.size();
    if (EVP_PKEY_get_raw_public_key(Key, Raw.data(), &Length) <= 0) {
        EVP_PKEY_free(Key);
        return std::nullopt;
    }
    Raw.resize(Length);
    Result.PublicKey = Base64Encode(Raw);

    Raw.assign(64, 0);
    Length = Raw.size();
    if (EVP_PKEY_get_raw_private_key(Key, Raw.data(), &Length) <= 0) {
        EVP_PKEY_free(Key);
        return std::nullopt;
    }
    Raw.resize(Length);
    Result.PrivateKey = Base64Encode(Raw);
    EVP_PKEY_free(Key);

    Result.KeyId = KeyIdFromPublic(Result.PublicKey);
    return Result;
}

std::optional<std::string> Crypto::Sign(const std::string& PrivateKey,
                                        const std::vector<unsigned char>& Message) {
    auto Raw = Base64Decode(PrivateKey);
    if (!Raw || Raw->size() != 32) return std::nullopt;
    EVP_PKEY* Key = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, Raw->data(), Raw->size());
    if (!Key) return std::nullopt;
    EVP_MD_CTX* Ctx = EVP_MD_CTX_new();
    if (!Ctx) {
        EVP_PKEY_free(Key);
        return std::nullopt;
    }
    std::vector<unsigned char> Signature(64);
    size_t Length = Signature.size();
    bool Ok = EVP_DigestSignInit(Ctx, nullptr, nullptr, nullptr, Key) > 0 &&
              EVP_DigestSign(Ctx, Signature.data(), &Length, Message.data(), Message.size()) > 0;
    EVP_MD_CTX_free(Ctx);
    EVP_PKEY_free(Key);
    if (!Ok) return std::nullopt;
    Signature.resize(Length);
    return Base64Encode(Signature);
}

bool Crypto::Verify(const std::string& PublicKey,
                    const std::vector<unsigned char>& Message,
                    const std::string& Signature) {
    auto RawKey = Base64Decode(PublicKey);
    auto RawSig = Base64Decode(Signature);
    if (!RawKey || !RawSig || RawKey->size() != 32) return false;
    EVP_PKEY* Key = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, RawKey->data(), RawKey->size());
    if (!Key) return false;
    EVP_MD_CTX* Ctx = EVP_MD_CTX_new();
    if (!Ctx) {
        EVP_PKEY_free(Key);
        return false;
    }
    bool Ok = EVP_DigestVerifyInit(Ctx, nullptr, nullptr, nullptr, Key) > 0 &&
              EVP_DigestVerify(Ctx, RawSig->data(), RawSig->size(), Message.data(), Message.size()) == 1;
    EVP_MD_CTX_free(Ctx);
    EVP_PKEY_free(Key);
    return Ok;
}

std::string Crypto::KeyIdFromPublic(const std::string& PublicKey) {
    std::vector<unsigned char> Data(PublicKey.begin(), PublicKey.end());
    return Sha256Hex(Data).substr(0, 16);
}

bool Keyring::load(const std::string& Path) {
    Records_.clear();
    std::ifstream Input(Path);
    if (!Input.is_open()) return false;
    std::string Line;
    while (std::getline(Input, Line)) {
        Line = Trim(Line);
        if (Line.empty() || Line[0] == '#') continue;
        auto Fields = Split(Line, '\t');
        if (Fields.size() < 7) continue;
        KeyRecord Record;
        Record.KeyId = Fields[0];
        Record.Role = Fields[1];
        Record.Name = Fields[2];
        Record.PublicKey = Fields[3];
        Record.Issued = Fields[4];
        Record.Expires = Fields[5];
        Record.CertifierId = Fields[6];
        if (Fields.size() > 7) Record.Signature = Fields[7];
        Records_.push_back(Record);
    }
    return true;
}

bool Keyring::save(const std::string& Path) const {
    auto Parent = fs::path(Path).parent_path();
    if (!Parent.empty()) {
        std::error_code Ec;
        fs::create_directories(Parent, Ec);
    }
    std::ofstream Output(Path);
    if (!Output.is_open()) return false;
    Output << "# Lunar keyring\n";
    for (const auto& Record : Records_) {
        Output << Record.KeyId << '\t'
               << Record.Role << '\t'
               << Record.Name << '\t'
               << Record.PublicKey << '\t'
               << Record.Issued << '\t'
               << Record.Expires << '\t'
               << Record.CertifierId << '\t'
               << Record.Signature << '\n';
    }
    return true;
}

void Keyring::add(const KeyRecord& Record) {
    for (auto& Existing : Records_) {
        if (Existing.KeyId == Record.KeyId) {
            Existing = Record;
            return;
        }
    }
    Records_.push_back(Record);
}

void Keyring::remove(const std::string& KeyId) {
    Records_.erase(std::remove_if(Records_.begin(), Records_.end(),
                                  [&](const KeyRecord& R) { return R.KeyId == KeyId; }),
                   Records_.end());
}

std::optional<KeyRecord> Keyring::find(const std::string& KeyId) const {
    for (const auto& Record : Records_) {
        if (Record.KeyId == KeyId) return Record;
    }
    return std::nullopt;
}

std::vector<std::string> Keyring::masters() const {
    std::vector<std::string> Result;
    for (const auto& Record : Records_) {
        if (Record.Role == "master") Result.push_back(Record.KeyId);
    }
    return Result;
}

std::vector<std::string> Keyring::developers() const {
    std::vector<std::string> Result;
    for (const auto& Record : Records_) {
        if (Record.Role == "developer") Result.push_back(Record.KeyId);
    }
    return Result;
}

std::string CanonicalCertBytes(const KeyRecord& Record) {
    std::ostringstream Out;
    Out << "okra-cert-v1\n"
        << "keyid=" << Record.KeyId << "\n"
        << "role=" << Record.Role << "\n"
        << "name=" << Record.Name << "\n"
        << "public=" << Record.PublicKey << "\n"
        << "issued=" << Record.Issued << "\n"
        << "expires=" << Record.Expires << "\n";
    return Out.str();
}

std::string CanonicalSignatureBytes(const SignatureRecord& Record) {
    std::ostringstream Out;
    Out << "okra-sig-v1\n"
        << "keyid=" << Record.KeyId << "\n"
        << "file=" << Record.FileName << "\n"
        << "sha256=" << Record.Sha256 << "\n";
    return Out.str();
}

bool WriteKeyFile(const std::string& Path, const std::string& Kind, const std::string& Key) {
    auto Parent = fs::path(Path).parent_path();
    if (!Parent.empty()) {
        std::error_code Ec;
        fs::create_directories(Parent, Ec);
    }
    std::ofstream Output(Path);
    if (!Output.is_open()) return false;
    Output << "kind=" << Kind << "\n"
           << "key=" << Key << "\n";
    return true;
}

std::optional<std::string> ReadKeyFile(const std::string& Path, const std::string& Kind) {
    std::ifstream Input(Path);
    if (!Input.is_open()) return std::nullopt;
    std::string FoundKind;
    std::string FoundKey;
    std::string Line;
    while (std::getline(Input, Line)) {
        Line = Trim(Line);
        if (Line.rfind("kind=", 0) == 0) FoundKind = Line.substr(5);
        else if (Line.rfind("key=", 0) == 0) FoundKey = Line.substr(4);
    }
    if (FoundKey.empty()) return std::nullopt;
    if (!Kind.empty() && FoundKind != Kind) return std::nullopt;
    return FoundKey;
}

bool WriteCertFile(const std::string& Path, const KeyRecord& Record) {
    auto Parent = fs::path(Path).parent_path();
    if (!Parent.empty()) {
        std::error_code Ec;
        fs::create_directories(Parent, Ec);
    }
    std::ofstream Output(Path);
    if (!Output.is_open()) return false;
    Output << "keyid: " << Record.KeyId << "\n"
           << "role: " << Record.Role << "\n"
           << "name: " << Record.Name << "\n"
           << "public: " << Record.PublicKey << "\n"
           << "issued: " << Record.Issued << "\n"
           << "expires: " << Record.Expires << "\n"
           << "certifier: " << Record.CertifierId << "\n"
           << "signature: " << Record.Signature << "\n";
    return true;
}

std::optional<KeyRecord> ReadCertFile(const std::string& Path) {
    std::ifstream Input(Path);
    if (!Input.is_open()) return std::nullopt;
    KeyRecord Record;
    std::string Line;
    while (std::getline(Input, Line)) {
        Line = Trim(Line);
        auto Colon = Line.find(':');
        if (Colon == std::string::npos) continue;
        std::string Key = Trim(Line.substr(0, Colon));
        std::string Value = Trim(Line.substr(Colon + 1));
        if (Key == "keyid") Record.KeyId = Value;
        else if (Key == "role") Record.Role = Value;
        else if (Key == "name") Record.Name = Value;
        else if (Key == "public") Record.PublicKey = Value;
        else if (Key == "issued") Record.Issued = Value;
        else if (Key == "expires") Record.Expires = Value;
        else if (Key == "certifier") Record.CertifierId = Value;
        else if (Key == "signature") Record.Signature = Value;
    }
    if (Record.KeyId.empty() || Record.PublicKey.empty()) return std::nullopt;
    return Record;
}

bool WriteSignatureFile(const std::string& Path, const SignatureRecord& Record) {
    auto Parent = fs::path(Path).parent_path();
    if (!Parent.empty()) {
        std::error_code Ec;
        fs::create_directories(Parent, Ec);
    }
    std::ofstream Output(Path);
    if (!Output.is_open()) return false;
    Output << "keyid: " << Record.KeyId << "\n"
           << "file: " << Record.FileName << "\n"
           << "sha256: " << Record.Sha256 << "\n"
           << "signature: " << Record.Signature << "\n";
    return true;
}

std::optional<SignatureRecord> ReadSignatureFile(const std::string& Path) {
    std::ifstream Input(Path);
    if (!Input.is_open()) return std::nullopt;
    SignatureRecord Record;
    std::string Line;
    while (std::getline(Input, Line)) {
        Line = Trim(Line);
        auto Colon = Line.find(':');
        if (Colon == std::string::npos) continue;
        std::string Key = Trim(Line.substr(0, Colon));
        std::string Value = Trim(Line.substr(Colon + 1));
        if (Key == "keyid") Record.KeyId = Value;
        else if (Key == "file") Record.FileName = Value;
        else if (Key == "sha256") Record.Sha256 = Value;
        else if (Key == "signature") Record.Signature = Value;
    }
    if (Record.KeyId.empty() || Record.Signature.empty()) return std::nullopt;
    return Record;
}

bool CertifyDeveloper(const std::string& MasterPrivate,
                      const std::string& MasterId,
                      const KeyRecord& Developer,
                      KeyRecord& Signed) {
    Signed = Developer;
    Signed.Role = "developer";
    Signed.CertifierId = MasterId;
    KeyRecord ForSigning = Signed;
    ForSigning.CertifierId = "";
    ForSigning.Signature = "";
    auto Bytes = CanonicalCertBytes(ForSigning);
    std::vector<unsigned char> Message(Bytes.begin(), Bytes.end());
    auto Signature = Crypto::Sign(MasterPrivate, Message);
    if (!Signature) return false;
    Signed.Signature = *Signature;
    return true;
}

bool VerifyCertification(const KeyRecord& Record, const std::string& MasterPublic) {
    if (Record.Signature.empty()) return false;
    KeyRecord Copy = Record;
    Copy.CertifierId = "";
    Copy.Signature = "";
    auto Bytes = CanonicalCertBytes(Copy);
    std::vector<unsigned char> Message(Bytes.begin(), Bytes.end());
    return Crypto::Verify(MasterPublic, Message, Record.Signature);
}

bool SignFile(const std::string& Path, const std::string& PrivateKey,
              const std::string& KeyId, SignatureRecord& Out) {
    std::string Digest = Crypto::Sha256File(Path);
    if (Digest.empty()) return false;
    Out.KeyId = KeyId;
    Out.FileName = fs::path(Path).filename().string();
    Out.Sha256 = Digest;
    auto Bytes = CanonicalSignatureBytes(Out);
    std::vector<unsigned char> Message(Bytes.begin(), Bytes.end());
    auto Signature = Crypto::Sign(PrivateKey, Message);
    if (!Signature) return false;
    Out.Signature = *Signature;
    return true;
}

bool VerifyFileSignature(const std::string& Path, const SignatureRecord& Record,
                         const std::string& PublicKey) {
    if (Record.Sha256.empty() || Record.Signature.empty()) return false;
    std::string Digest = Crypto::Sha256File(Path);
    if (Digest != Record.Sha256) return false;
    auto Bytes = CanonicalSignatureBytes(Record);
    std::vector<unsigned char> Message(Bytes.begin(), Bytes.end());
    return Crypto::Verify(PublicKey, Message, Record.Signature);
}

}

#endif
