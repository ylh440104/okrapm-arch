#pragma once
#include <string>
#include <vector>
#include <optional>

namespace okrapm {

struct KeyPair {
    std::string PublicKey;
    std::string PrivateKey;
    std::string KeyId;
};

struct KeyRecord {
    std::string KeyId;
    std::string PublicKey;
    std::string Name;
    std::string Role;
    std::string Issued;
    std::string Expires;
    std::string CertifierId;
    std::string Signature;
};

struct SignatureRecord {
    std::string KeyId;
    std::string FileName;
    std::string Sha256;
    std::string Signature;
};

class Crypto {
public:
    static std::string Base64Encode(const std::vector<unsigned char>& Data);
    static std::optional<std::vector<unsigned char>> Base64Decode(const std::string& Text);
    static std::string Sha256Hex(const std::vector<unsigned char>& Data);
    static std::string Sha256File(const std::string& Path);
    static std::string NowStamp();
    static std::optional<KeyPair> Generate();
    static std::optional<std::string> Sign(const std::string& PrivateKey,
                                           const std::vector<unsigned char>& Message);
    static bool Verify(const std::string& PublicKey,
                       const std::vector<unsigned char>& Message,
                       const std::string& Signature);
    static std::string KeyIdFromPublic(const std::string& PublicKey);
};

class Keyring {
public:
    bool load(const std::string& Path);
    bool save(const std::string& Path) const;
    void add(const KeyRecord& Record);
    void remove(const std::string& KeyId);
    std::optional<KeyRecord> find(const std::string& KeyId) const;
    std::vector<KeyRecord> all() const { return Records_; }
    std::vector<std::string> masters() const;
    std::vector<std::string> developers() const;
    bool empty() const { return Records_.empty(); }
    size_t size() const { return Records_.size(); }
private:
    std::vector<KeyRecord> Records_;
};

std::string CanonicalCertBytes(const KeyRecord& Record);
std::string CanonicalSignatureBytes(const SignatureRecord& Record);

bool WriteKeyFile(const std::string& Path, const std::string& Kind, const std::string& Key);
std::optional<std::string> ReadKeyFile(const std::string& Path, const std::string& Kind);
bool WriteCertFile(const std::string& Path, const KeyRecord& Record);
std::optional<KeyRecord> ReadCertFile(const std::string& Path);
bool WriteSignatureFile(const std::string& Path, const SignatureRecord& Record);
std::optional<SignatureRecord> ReadSignatureFile(const std::string& Path);

bool CertifyDeveloper(const std::string& MasterPrivate,
                      const std::string& MasterId,
                      const KeyRecord& Developer,
                      KeyRecord& Signed);

bool VerifyCertification(const KeyRecord& Record, const std::string& MasterPublic);

bool SignFile(const std::string& Path, const std::string& PrivateKey,
              const std::string& KeyId, SignatureRecord& Out);

bool VerifyFileSignature(const std::string& Path, const SignatureRecord& Record,
                         const std::string& PublicKey);

}
