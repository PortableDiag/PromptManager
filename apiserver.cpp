#include "apiserver.h"
#include "mainwindow.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>
#include <QHostAddress>
#include <QStringList>

ApiResponse ApiResponse::error(int code, const QString &message)
{
    QJsonObject b;
    b["error"] = message;
    b["status"] = code;
    return {code, b};
}

ApiServer::ApiServer(MainWindow *window, QObject *parent)
    : QObject(parent), m_window(window)
{
}

ApiServer::~ApiServer()
{
    stop();
}

bool ApiServer::isRunning() const
{
    return m_server && m_server->isListening();
}

quint16 ApiServer::port() const
{
    return m_server ? m_server->serverPort() : 0;
}

bool ApiServer::start(quint16 port, const QString &apiKey, QString *errorOut)
{
    stop();

    m_apiKey = apiKey;
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &ApiServer::onNewConnection);

    // Bind to loopback only: the API is meant for local agents/tools, not the
    // open network. Binding to all interfaces would expose the store to the LAN.
    if (!m_server->listen(QHostAddress::LocalHost, port)) {
        if (errorOut)
            *errorOut = m_server->errorString();
        m_server->deleteLater();
        m_server = nullptr;
        return false;
    }
    return true;
}

void ApiServer::stop()
{
    if (!m_server)
        return;

    for (auto it = m_buffers.begin(); it != m_buffers.end(); ++it)
        it.key()->deleteLater();
    m_buffers.clear();

    m_server->close();
    m_server->deleteLater();
    m_server = nullptr;
}

void ApiServer::onNewConnection()
{
    while (m_server && m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        m_buffers.insert(socket, QByteArray());
        connect(socket, &QTcpSocket::readyRead, this, &ApiServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &ApiServer::onDisconnected);
    }
}

void ApiServer::onDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;
    m_buffers.remove(socket);
    socket->deleteLater();
}

void ApiServer::onReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;
    m_buffers[socket].append(socket->readAll());
    processBuffer(socket);
}

// Parse a simple HTTP/1.1 request once the full request (headers + body) has
// arrived, dispatch it, and close the connection (no keep-alive).
void ApiServer::processBuffer(QTcpSocket *socket)
{
    QByteArray &buffer = m_buffers[socket];

    int headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0)
        return; // headers not complete yet

    const QByteArray headerBlock = buffer.left(headerEnd);
    const QList<QByteArray> lines = headerBlock.split('\n');
    if (lines.isEmpty())
        return;

    // Request line: METHOD PATH HTTP/1.1
    const QList<QByteArray> requestLine = lines.first().trimmed().split(' ');
    if (requestLine.size() < 2) {
        sendResponse(socket, ApiResponse::error(400, "Malformed request"), false);
        return;
    }
    const QString method = QString::fromUtf8(requestLine[0]).toUpper();
    const QString target = QString::fromUtf8(requestLine[1]);

    // Parse headers (case-insensitive keys).
    QHash<QString, QString> headers;
    for (int i = 1; i < lines.size(); ++i) {
        const QByteArray &line = lines[i];
        int colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        const QString key = QString::fromUtf8(line.left(colon)).trimmed().toLower();
        const QString value = QString::fromUtf8(line.mid(colon + 1)).trimmed();
        headers.insert(key, value);
    }

    // Wait for the full body if Content-Length says there's more coming.
    int contentLength = headers.value("content-length", "0").toInt();
    int bodyStart = headerEnd + 4;
    if (buffer.size() - bodyStart < contentLength)
        return; // body not fully received yet
    const QByteArray body = buffer.mid(bodyStart, contentLength);

    // Split path and query string.
    QUrl url(target);
    const QString path = url.path();
    QHash<QString, QString> query;
    const QList<QPair<QString, QString>> items =
        QUrlQuery(url).queryItems(QUrl::FullyDecoded);
    for (const auto &kv : items)
        query.insert(kv.first, kv.second);

    // CORS preflight — answer without auth so browser-based agents can connect.
    if (method == "OPTIONS") {
        sendResponse(socket, {204, QJsonObject()}, true);
        return;
    }

    // Authenticate: Authorization: Bearer <key>  OR  X-API-Key: <key>.
    bool authed = false;
    if (!m_apiKey.isEmpty()) {
        const QString auth = headers.value("authorization");
        if (auth.startsWith("Bearer ", Qt::CaseInsensitive))
            authed = (auth.mid(7).trimmed() == m_apiKey);
        if (!authed)
            authed = (headers.value("x-api-key") == m_apiKey);
    }

    const ApiResponse resp = route(method, path, query, body, authed);
    sendResponse(socket, resp, true);
}

ApiResponse ApiServer::route(const QString &method, const QString &path,
                             const QHash<QString, QString> &query,
                             const QByteArray &body, bool authed)
{
    // Health check is unauthenticated so tools can probe connectivity.
    if (path == "/api/health" || path == "/health") {
        QJsonObject b;
        b["status"] = "ok";
        b["service"] = "prompt-manager";
        b["version"] = "2.5.2";
        return ApiResponse::ok(b);
    }

    if (!authed)
        return ApiResponse::error(401, "Missing or invalid API key");

    // Parse a JSON object body when present.
    QJsonObject input;
    if (!body.isEmpty()) {
        QJsonParseError perr;
        QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
        if (perr.error != QJsonParseError::NoError)
            return ApiResponse::error(400, "Invalid JSON body: " + perr.errorString());
        // Valid JSON that isn't an object (an array, a bare string, a number)
        // used to leave `input` empty and fall through as a no-op 200 — the
        // same silent-success trap as an unknown field name.
        if (!doc.isObject())
            return ApiResponse::error(400, "Request body must be a JSON object");
        input = doc.object();
    }

    const QStringList parts = path.split('/', Qt::SkipEmptyParts); // e.g. api, prompts, {id}
    if (parts.isEmpty() || parts[0] != "api")
        return ApiResponse::error(404, "Unknown endpoint");

    const QString resource = parts.value(1);
    const QString ident = parts.value(2);

    if (resource == "prompts") {
        if (ident.isEmpty()) {
            if (method == "GET")
                return m_window->apiListPrompts(query.value("folder"), query.value("q"));
            if (method == "POST")
                return m_window->apiCreatePrompt(input);
            return ApiResponse::error(405, "Method not allowed");
        }
        if (method == "GET")
            return m_window->apiGetPrompt(ident);
        if (method == "PUT" || method == "PATCH")
            return m_window->apiUpdatePrompt(ident, input);
        if (method == "DELETE")
            return m_window->apiDeletePrompt(ident);
        return ApiResponse::error(405, "Method not allowed");
    }

    if (resource == "folders") {
        if (method == "GET")
            return m_window->apiListFolders();
        if (method == "POST")
            return m_window->apiCreateFolder(input);
        if (method == "DELETE")
            return m_window->apiDeleteFolder(query.value("path"));
        return ApiResponse::error(405, "Method not allowed");
    }

    return ApiResponse::error(404, "Unknown endpoint");
}

void ApiServer::sendResponse(QTcpSocket *socket, const ApiResponse &resp, bool cors)
{
    static const QHash<int, QString> reasons = {
        {200, "OK"}, {201, "Created"}, {204, "No Content"},
        {400, "Bad Request"}, {401, "Unauthorized"}, {404, "Not Found"},
        {405, "Method Not Allowed"}, {409, "Conflict"}, {500, "Internal Server Error"}
    };

    QByteArray payload;
    if (resp.status != 204)
        payload = QJsonDocument(resp.body).toJson(QJsonDocument::Compact);

    const QString reason = reasons.value(resp.status, "OK");
    QByteArray out;
    out += "HTTP/1.1 " + QByteArray::number(resp.status) + ' ' + reason.toUtf8() + "\r\n";
    out += "Content-Type: application/json\r\n";
    out += "Content-Length: " + QByteArray::number(payload.size()) + "\r\n";
    if (cors) {
        out += "Access-Control-Allow-Origin: *\r\n";
        out += "Access-Control-Allow-Methods: GET, POST, PUT, PATCH, DELETE, OPTIONS\r\n";
        out += "Access-Control-Allow-Headers: Authorization, Content-Type, X-API-Key\r\n";
    }
    out += "Connection: close\r\n";
    out += "\r\n";
    out += payload;

    socket->write(out);
    socket->flush();
    socket->disconnectFromHost();
}
