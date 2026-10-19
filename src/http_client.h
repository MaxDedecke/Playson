#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <winhttp.h>
#include <vector>
#include <string>
#include <cstdint>
#include <functional>
#include <thread>

#pragma comment(lib, "winhttp.lib")

namespace playson {

class HttpClient {
public:
    static void sendWavAsync(const std::vector<uint8_t>& wavData,
                             const std::string& filename,
                             uint16_t port,
                             std::function<void(bool, std::string)> onComplete) {
        // Run in separate worker thread to avoid blocking audio/GUI threads
        std::thread([wavData, filename, port, onComplete]() {
            bool success = false;
            std::string message;

            HINTERNET hSession = WinHttpOpen(L"Playson-Plugin/1.0",
                                            WINHTTP_ACCESS_TYPE_NO_PROXY,
                                            WINHTTP_NO_PROXY_NAME,
                                            WINHTTP_NO_PROXY_BYPASS,
                                            0);
            if (!hSession) {
                if (onComplete) onComplete(false, "WinHttpOpen failed");
                return;
            }

            HINTERNET hConnect = WinHttpConnect(hSession, L"127.0.0.1", port, 0);
            if (!hConnect) {
                WinHttpCloseHandle(hSession);
                if (onComplete) onComplete(false, "Konnte Verbindung zu localhost:5005 nicht herstellen");
                return;
            }

            HINTERNET hRequest = WinHttpOpenRequest(hConnect,
                                                   L"POST",
                                                   L"/api/upload",
                                                   NULL,
                                                   NULL,
                                                   NULL,
                                                   0);
            if (!hRequest) {
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                if (onComplete) onComplete(false, "Request-Erstellung fehlgeschlagen");
                return;
            }

            // Build multipart/form-data payload
            std::string boundary = "----PlaysonBoundary7MA4YWxkTrZu0gW";
            std::string headerStr = "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";

            std::string bodyStart;
            bodyStart += "--" + boundary + "\r\n";
            bodyStart += "Content-Disposition: form-data; name=\"file\"; filename=\"" + filename + "\"\r\n";
            bodyStart += "Content-Type: audio/wav\r\n\r\n";

            std::string bodyEnd;
            bodyEnd += "\r\n--" + boundary + "--\r\n";

            std::vector<uint8_t> fullPayload;
            fullPayload.reserve(bodyStart.size() + wavData.size() + bodyEnd.size());
            fullPayload.insert(fullPayload.end(), bodyStart.begin(), bodyStart.end());
            fullPayload.insert(fullPayload.end(), wavData.begin(), wavData.end());
            fullPayload.insert(fullPayload.end(), bodyEnd.begin(), bodyEnd.end());

            std::wstring wHeader(headerStr.begin(), headerStr.end());

            BOOL bResults = WinHttpSendRequest(hRequest,
                                               wHeader.c_str(),
                                               static_cast<DWORD>(wHeader.length()),
                                               fullPayload.data(),
                                               static_cast<DWORD>(fullPayload.size()),
                                               static_cast<DWORD>(fullPayload.size()),
                                               0);

            if (bResults) {
                bResults = WinHttpReceiveResponse(hRequest, NULL);
                if (bResults) {
                    DWORD statusCode = 0;
                    DWORD size = sizeof(statusCode);
                    WinHttpQueryHeaders(hRequest,
                                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                        WINHTTP_HEADER_NAME_BY_INDEX,
                                        &statusCode,
                                        &size,
                                        WINHTTP_NO_HEADER_INDEX);

                    if (statusCode == 200) {
                        success = true;
                        message = "Erfolgreich an Sonos gesendet!";
                    } else {
                        message = "Server antwortete mit Code: " + std::to_string(statusCode);
                    }
                } else {
                    message = "Keine Antwort vom Sonos Connector empfangen";
                }
            } else {
                message = "Senden fehlgeschlagen (Connector laeuft nicht?)";
            }

            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);

            if (onComplete) {
                onComplete(success, message);
            }
        }).detach();
    }
};

} // namespace playson
