#pragma once
#include <string>
#include <vector>
// Configurações Globais de Licenciamento do Structura Client.
// Fornece chaves públicas autoritativas embutidas, URLs padrão e persistência de sessão.

#include <cstdint>
#include <string>
#include "license_client.h"

namespace structura_license {

// Retorna a URL base do servidor de licenciamento (ex: "http://127.0.0.1:8000").
// Respeita a variável de ambiente STRUCTURA_LICENSE_SERVER se definida.
std::string GetLicenseServerUrl();

// Retorna o ponteiro para os 32 bytes da chave pública Ed25519 de Release (assinatura de manifestos).
const uint8_t* GetReleasePublicKey();

// Retorna o ponteiro para os 32 bytes da chave pública Ed25519 do Servidor (Keystone e Nível B).
const uint8_t* GetServerPublicKey();

// Pins SPKI SHA-256 (hex) aceitos para o TLS do servidor; vazio = sem pinning.
const std::vector<std::string>& GetServerCertPins();

// Retorna o diretório de instalação do aplicativo (onde estão rds.enc, mefs.enc, manifest.txt).
std::string GetAppInstallDir();

// Caminho do arquivo de sessão no AppData do usuário
std::string GetSessionFilePath();

// Persistência segura de sessão ativa
bool SaveSessionToDisk(const ClientSession& session);
bool LoadSessionFromDisk(ClientSession& outSession);
void ClearSessionOnDisk();

} // namespace structura_license
