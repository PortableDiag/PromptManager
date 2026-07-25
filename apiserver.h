#ifndef APISERVER_H
#define APISERVER_H

#include <QObject>
#include <QJsonObject>
#include <QHash>

class QTcpServer;
class QTcpSocket;
class MainWindow;

// Result of an API call: an HTTP status code plus a JSON body.
struct ApiResponse {
    int status = 200;
    QJsonObject body;

    static ApiResponse ok(const QJsonObject &b = QJsonObject()) { return {200, b}; }
    static ApiResponse created(const QJsonObject &b) { return {201, b}; }
    static ApiResponse error(int code, const QString &message);
};

// A tiny HTTP/1.1 server (built on QTcpServer) that exposes the prompt store
// over a local REST API. It lives on the GUI thread's event loop, so handlers
// may touch the models and widgets directly without synchronisation.
class ApiServer : public QObject
{
    Q_OBJECT

public:
    explicit ApiServer(MainWindow *window, QObject *parent = nullptr);
    ~ApiServer();

    // Starts listening on the given port. Returns false and fills errorOut on
    // failure (e.g. port already in use). Restarts cleanly if already running.
    bool start(quint16 port, const QString &apiKey, QString *errorOut = nullptr);
    void stop();

    bool isRunning() const;
    quint16 port() const;
    void setApiKey(const QString &key) { m_apiKey = key; }

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    void processBuffer(QTcpSocket *socket);
    ApiResponse route(const QString &method, const QString &path,
                      const QHash<QString, QString> &query,
                      const QByteArray &body, bool authed);
    void sendResponse(QTcpSocket *socket, const ApiResponse &resp, bool cors);

    QTcpServer *m_server = nullptr;
    MainWindow *m_window = nullptr;
    QString m_apiKey;
    QHash<QTcpSocket *, QByteArray> m_buffers;
};

#endif // APISERVER_H
