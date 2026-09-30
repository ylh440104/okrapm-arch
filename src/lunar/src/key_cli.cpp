#include "key_cli.h"
#include "okrapmlib/crypto.h"
#include <iostream>

using namespace okrapm;

namespace {

std::string FlagValue(const std::vector<std::string>& Args, const std::string& Flag) {
    for (size_t i = 0; i + 1 < Args.size(); ++i) {
        if (Args[i] == Flag) return Args[i + 1];
    }
    return {};
}

int KeyGenerate(const std::vector<std::string>& Args) {
    std::string Type = FlagValue(Args, "--type");
    std::string PrivOut = FlagValue(Args, "--private");
    std::string PubOut = FlagValue(Args, "--public");
    std::string MasterKey = FlagValue(Args, "--master-key");
    std::string Name = FlagValue(Args, "--name");
    std::string CertOut = FlagValue(Args, "--cert");
    std::string OldKind = FlagValue(Args, "--kind");
    std::string OldOut = FlagValue(Args, "--out");
    if (!OldOut.empty() && PrivOut.empty() && PubOut.empty()) {
        if (OldKind == "public") PubOut = OldOut;
        else PrivOut = OldOut;
    }
    if (Type.empty()) {
        if (MasterKey.empty()) {
            Type = "master";
        } else {
            Type = "developer";
        }
    }
    if (PrivOut.empty() && PubOut.empty()) {
        if (Type == "master") {
            std::cerr << "Usage: lunar key generate --type master --private <path> --public <path>\n";
        } else {
            std::cerr << "Usage: lunar key generate --type developer --master-key <path> --name <n> --private <path> --public <path> --cert <path>\n";
        }
        return 1;
    }
    if (Type == "developer") {
        if (MasterKey.empty()) {
            std::cerr << "Developer key generation requires --master-key\n";
            return 1;
        }
        if (Name.empty()) {
            std::cerr << "Developer key generation requires --name\n";
            return 1;
        }
        if (CertOut.empty()) {
            std::cerr << "Developer key generation requires --cert\n";
            return 1;
        }
    }
    auto MasterPriv = std::optional<std::string>{};
    if (Type == "developer") {
        MasterPriv = ReadKeyFile(MasterKey, "private");
        if (!MasterPriv) {
            std::cerr << "Cannot read master key: " << MasterKey << "\n";
            return 1;
        }
    }
    auto Pair = Crypto::Generate();
    if (!Pair) {
        std::cerr << "Key generation failed\n";
        return 1;
    }
    if (!PrivOut.empty()) {
        if (!WriteKeyFile(PrivOut, "private", Pair->PrivateKey)) {
            std::cerr << "Cannot write " << PrivOut << "\n";
            return 1;
        }
    }
    if (!PubOut.empty()) {
        if (!WriteKeyFile(PubOut, "public", Pair->PublicKey)) {
            std::cerr << "Cannot write " << PubOut << "\n";
            return 1;
        }
    }
    std::cout << "KeyId: " << Pair->KeyId << "\n";
    if (Type == "developer") {
        KeyRecord Record;
        Record.PublicKey = Pair->PublicKey;
        Record.KeyId = Pair->KeyId;
        Record.Name = Name;
        Record.Issued = Crypto::NowStamp();
        Record.Expires = "";
        KeyRecord Signed;
        if (!CertifyDeveloper(*MasterPriv, Pair->KeyId, Record, Signed)) {
            std::cerr << "Certification failed\n";
            return 1;
        }
        Signed.CertifierId = "";
        if (!WriteCertFile(CertOut, Signed)) {
            std::cerr << "Cannot write certificate: " << CertOut << "\n";
            return 1;
        }
        std::cout << "Certified by master key\n";
        std::cout << "Certificate: " << CertOut << "\n";
    }
    return 0;
}

int KeyId(const std::vector<std::string>& Args) {
    std::string Key = FlagValue(Args, "--key");
    if (Key.empty()) {
        std::cerr << "Usage: lunar key id --key <path>\n";
        return 1;
    }
    auto Value = ReadKeyFile(Key, "");
    if (!Value) {
        std::cerr << "Cannot read " << Key << "\n";
        return 1;
    }
    std::cout << Crypto::KeyIdFromPublic(*Value) << "\n";
    return 0;
}

int KeyCertify(const std::vector<std::string>& Args) {
    std::string Master = FlagValue(Args, "--master");
    std::string Developer = FlagValue(Args, "--developer");
    std::string Name = FlagValue(Args, "--name");
    std::string Out = FlagValue(Args, "--out");
    if (Master.empty() || Developer.empty() || Out.empty()) {
        std::cerr << "Usage: lunar key certify --master <key> --developer <pub> --name <n> --out <cert>\n";
        return 1;
    }
    auto MasterKey = ReadKeyFile(Master, "private");
    auto DevKey = ReadKeyFile(Developer, "public");
    if (!MasterKey || !DevKey) {
        std::cerr << "Cannot read key files\n";
        return 1;
    }
    KeyRecord Record;
    Record.PublicKey = *DevKey;
    Record.KeyId = Crypto::KeyIdFromPublic(*DevKey);
    Record.Name = Name;
    Record.Issued = Crypto::NowStamp();
    Record.Expires = "";
    KeyRecord Signed;
    if (!CertifyDeveloper(*MasterKey, Crypto::KeyIdFromPublic(*DevKey), Record, Signed)) {
        std::cerr << "Certification failed\n";
        return 1;
    }
    Signed.CertifierId = "";
    if (!WriteCertFile(Out, Signed)) {
        std::cerr << "Cannot write " << Out << "\n";
        return 1;
    }
    std::cout << "Certified: " << Signed.KeyId << "\n";
    return 0;
}

int KeySign(const std::vector<std::string>& Args) {
    std::string Key = FlagValue(Args, "--key");
    std::string File = FlagValue(Args, "--file");
    std::string Out = FlagValue(Args, "--out");
    if (Key.empty() || File.empty() || Out.empty()) {
        std::cerr << "Usage: lunar key sign --key <priv> --file <f> --out <sig>\n";
        return 1;
    }
    auto PrivateKey = ReadKeyFile(Key, "private");
    if (!PrivateKey) {
        std::cerr << "Cannot read " << Key << "\n";
        return 1;
    }
    SignatureRecord Record;
    if (!SignFile(File, *PrivateKey, Crypto::KeyIdFromPublic(*PrivateKey), Record)) {
        std::cerr << "Signing failed\n";
        return 1;
    }
    if (!WriteSignatureFile(Out, Record)) {
        std::cerr << "Cannot write " << Out << "\n";
        return 1;
    }
    std::cout << "Signed: " << File << "\n";
    std::cout << "sha256: " << Record.Sha256 << "\n";
    return 0;
}

int KeyVerify(const std::vector<std::string>& Args) {
    std::string RingPath = FlagValue(Args, "--keyring");
    std::string File = FlagValue(Args, "--file");
    std::string Sig = FlagValue(Args, "--sig");
    if (RingPath.empty() || File.empty() || Sig.empty()) {
        std::cerr << "Usage: lunar key verify --keyring <k> --file <f> --sig <s>\n";
        return 1;
    }
    Keyring Ring;
    Ring.load(RingPath);
    auto Record = ReadSignatureFile(Sig);
    if (!Record) {
        std::cerr << "Cannot read signature\n";
        return 1;
    }
    for (const auto& Candidate : Ring.all()) {
        if (VerifyFileSignature(File, *Record, Candidate.PublicKey)) {
            std::cout << ":: signature valid, signer " << Candidate.Name
                      << " (" << Candidate.Role << ")\n";
            return 0;
        }
    }
    std::cerr << "Signature verification failed\n";
    return 1;
}

int KeyringList(const std::string& Path, Keyring& Ring) {
    if (Ring.empty()) {
        std::cout << "Keyring is empty\n";
        return 0;
    }
    for (const auto& Record : Ring.all()) {
        std::cout << Record.KeyId << "  " << Record.Role << "  " << Record.Name
                  << "  issued " << Record.Issued << "\n";
    }
    return 0;
}

int KeyringAdd(const std::vector<std::string>& Args, const std::string& Path, Keyring& Ring) {
    std::string Cert = FlagValue(Args, "--cert");
    if (Cert.empty()) {
        std::cerr << "Usage: lunar keyring add --keyring <k> --cert <c>\n";
        return 1;
    }
    auto Record = ReadCertFile(Cert);
    if (!Record) {
        std::cerr << "Cannot read certificate\n";
        return 1;
    }
    if (Record->Role != "master") {
        bool Certified = false;
        for (const auto& Id : Ring.masters()) {
            auto MasterRecord = Ring.find(Id);
            if (!MasterRecord) continue;
            if (VerifyCertification(*Record, MasterRecord->PublicKey)) {
                Certified = true;
                break;
            }
        }
        if (!Certified) {
            std::cerr << "Certificate is not signed by any known master key\n";
            return 1;
        }
    }
    Ring.add(*Record);
    if (!Ring.save(Path)) {
        std::cerr << "Cannot write " << Path << "\n";
        return 1;
    }
    std::cout << "Added " << Record->KeyId << " to " << Path << "\n";
    return 0;
}

}

int okrapm::RunKeyCommand(const std::vector<std::string>& Args) {
    std::string Sub = Args.size() > 1 ? Args[1] : "";
    if (Sub == "generate") return KeyGenerate(Args);
    if (Sub == "id") return KeyId(Args);
    if (Sub == "certify") return KeyCertify(Args);
    if (Sub == "sign") return KeySign(Args);
    if (Sub == "verify") return KeyVerify(Args);
    std::cerr << "Usage: lunar key <generate|id|certify|sign|verify>\n"
              << "  generate --type master --private <p> --public <p>\n"
              << "  generate --type developer --master-key <p> --name <n> --private <p> --public <p> --cert <p>\n";
    return 1;
}

int okrapm::RunKeyringCommand(const std::vector<std::string>& Args) {
    std::string Sub = Args.size() > 1 ? Args[1] : "";
    std::string Path = FlagValue(Args, "--keyring");
    if (Path.empty()) {
        std::cerr << "Usage: lunar keyring <list|add> --keyring <path>\n";
        return 1;
    }
    Keyring Ring;
    Ring.load(Path);
    if (Sub == "list") return KeyringList(Path, Ring);
    if (Sub == "add") return KeyringAdd(Args, Path, Ring);
    std::cerr << "Usage: lunar keyring <list|add> --keyring <path>\n";
    return 1;
}