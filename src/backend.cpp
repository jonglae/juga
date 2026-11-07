#include "backend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrlQuery>
#include <QUuid>

namespace {
const QUrl apiBase("https://openapi.tossinvest.com");
const QUrl socketUrl("wss://openapi-ws.tossinvest.com/ws/v1");
QString readableError(QNetworkReply *reply) {
    const auto doc = QJsonDocument::fromJson(reply->readAll());
    const auto error = doc.object().value("error").toObject();
    const QString message = error.value("message").toString();
    return message.isEmpty() ? reply->errorString() : message;
}
QString displayTime(const QString &iso) {
    const QDateTime date = QDateTime::fromString(iso, Qt::ISODateWithMs);
    if (date.isValid()) return date.toLocalTime().toString("yyyy-MM-dd HH:mm:ss");
    const QDateTime simple = QDateTime::fromString(iso, Qt::ISODate);
    return simple.isValid() ? simple.toLocalTime().toString("yyyy-MM-dd HH:mm:ss") : iso;
}
}

Backend::Backend(QObject *parent) : QObject(parent) {
    loadSettings();
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this] {
        if (configured() && !m_symbol.isEmpty()) loadAccounts();
    });
    connect(&m_socket, &QWebSocket::connected, this, [this] {
        m_reconnectDelay = 1000;
        m_pingTimer.start(60000);
        QJsonArray declaration;
        QJsonObject item;
        item.insert("type", "trade:kr");
        item.insert("codes", QJsonArray{m_symbol});
        declaration.append(item);
        m_socket.sendTextMessage(QString::fromUtf8(QJsonDocument(declaration).toJson(QJsonDocument::Compact)));
        setStatus(tr("실시간 시세 연결됨 · 체결 대기 중"));
    });
    connect(&m_socket, &QWebSocket::textMessageReceived, this, [this](const QString &message) {
        const auto doc = QJsonDocument::fromJson(message.toUtf8());
        if (!doc.isObject()) return;
        const auto frame = doc.object();
        if (frame.value("type").toString() == "message") {
            const auto data = frame.value("data").toObject();
            if (!data.value("price").toString().isEmpty()) {
                m_price = data.value("price").toString();
                m_tradeTime = displayTime(data.value("timestamp").toString());
                emit changed();
            }
        } else if (frame.value("type").toString() == "subscriptions") {
            const QJsonArray rejected = frame.value("rejected").toArray();
            if (!rejected.isEmpty())
                setStatus(tr("종목 구독 실패: %1").arg(rejected.first().toObject().value("message").toString()));
        } else if (frame.value("type").toString() == "error") {
            setStatus(tr("실시간 시세 오류: %1").arg(frame.value("error").toObject().value("message").toString()));
        }
    });
    connect(&m_socket, &QWebSocket::disconnected, this, [this] {
        m_pingTimer.stop();
        if (m_manualSocketChange) { m_manualSocketChange = false; return; }
        if (!m_symbol.isEmpty() && configured()) {
            setStatus(tr("실시간 연결이 끊겼습니다. 다시 연결합니다…"));
            m_reconnectTimer.start(m_reconnectDelay);
            m_reconnectDelay = qMin(m_reconnectDelay * 2, 30000);
        }
    });
    connect(&m_socket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        setStatus(tr("실시간 연결 실패: %1").arg(m_socket.errorString()));
    });
    connect(&m_pingTimer, &QTimer::timeout, this, [this] {
        if (m_socket.state() == QAbstractSocket::ConnectedState) m_socket.ping();
    });
    if (configured()) loadAccounts();
    else setStatus(tr("토스 Open API 키를 설정해 주세요."));
}

void Backend::loadSettings() {
    QSettings settings(QDir::homePath() + "/.config/juga/settings.ini", QSettings::IniFormat);
    m_clientId = settings.value("toss/clientId").toString();
    m_clientSecret = settings.value("toss/clientSecret").toString();
    m_selectedAccount = settings.value("toss/account").toString();
    m_symbol = settings.value("market/symbol", "005930").toString();
}

void Backend::saveSettings() {
    const QString path = QDir::homePath() + "/.config/juga/settings.ini";
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSettings settings(path, QSettings::IniFormat);
    settings.setValue("toss/clientId", m_clientId);
    settings.setValue("toss/clientSecret", m_clientSecret);
    settings.setValue("toss/account", m_selectedAccount);
    settings.setValue("market/symbol", m_symbol);
    settings.sync();
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
}

void Backend::saveCredentials(const QString &clientId, const QString &clientSecret) {
    m_clientId = clientId.trimmed();
    m_clientSecret = clientSecret.trimmed();
    m_token.clear();
    saveSettings();
    emit changed();
    loadAccounts();
}

void Backend::setSelectedAccount(const QString &account) {
    if (m_selectedAccount == account) return;
    m_selectedAccount = account;
    saveSettings();
    emit changed();
}

QNetworkRequest Backend::authorizedRequest(const QUrl &url, const QString &token) const {
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(15000);
    return request;
}

void Backend::ensureToken(std::function<void(const QString &)> ready) {
    if (!m_token.isEmpty()) { ready(m_token); return; }
    if (!configured()) { setStatus(tr("토스 Open API 키를 먼저 설정해 주세요.")); return; }
    QNetworkRequest request(apiBase.resolved(QUrl("/oauth2/token")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QUrlQuery form;
    form.addQueryItem("grant_type", "client_credentials");
    form.addQueryItem("client_id", m_clientId);
    form.addQueryItem("client_secret", m_clientSecret);
    auto *reply = m_network.post(request, form.query(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply, ready] {
        if (reply->error() != QNetworkReply::NoError) {
            setStatus(tr("토큰 발급 실패: %1").arg(readableError(reply)));
            reply->deleteLater(); return;
        }
        const auto object = QJsonDocument::fromJson(reply->readAll()).object();
        m_token = object.value("access_token").toString();
        reply->deleteLater();
        if (m_token.isEmpty()) { setStatus(tr("토스 API가 액세스 토큰을 반환하지 않았습니다.")); return; }
        ready(m_token);
    });
}

void Backend::get(const QString &path, const QString &query, const QString &token,
                  std::function<void(QNetworkReply *)> callback) {
    QUrl url = apiBase.resolved(QUrl(path));
    if (!query.isEmpty()) url.setQuery(query);
    auto *reply = m_network.get(authorizedRequest(url, token));
    connect(reply, &QNetworkReply::finished, this, [reply, callback] { callback(reply); });
}

void Backend::loadAccounts() {
    ensureToken([this](const QString &token) { fetchAccountList(token); });
}

void Backend::fetchAccountList(const QString &token) {
    setStatus(tr("토스 계좌를 불러오는 중…"));
    get("/api/v1/accounts", {}, token, [this](QNetworkReply *reply) {
        if (reply->error() != QNetworkReply::NoError) {
            setStatus(tr("계좌 조회 실패: %1").arg(readableError(reply)));
            reply->deleteLater(); return;
        }
        const auto object = QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonArray rows = object.value("result").toArray();
        m_accounts.clear();
        for (const auto &entry : rows) {
            const auto account = entry.toObject();
            const QString seq = account.value("accountSeq").toVariant().toString();
            if (seq.isEmpty()) continue;
            m_accounts.append(QVariantMap{{"seq", seq}, {"label", account.value("name").toString(tr("토스 계좌 %1").arg(seq))}});
        }
        if (m_selectedAccount.isEmpty() && !m_accounts.isEmpty())
            m_selectedAccount = m_accounts.first().toMap().value("seq").toString();
        saveSettings();
        emit changed();
        if (m_accounts.isEmpty()) setStatus(tr("사용 가능한 계좌가 없습니다. 토스 API 권한과 계좌를 확인해 주세요."));
        else {
            setStatus(tr("계좌 연결됨 · 종목 코드를 입력하세요."));
            if (!m_symbol.isEmpty()) watchSymbol(m_symbol);
        }
        reply->deleteLater();
    });
}

void Backend::watchSymbol(const QString &raw) {
    const QString code = raw.trimmed().toUpper();
    static const QRegularExpression valid("^[A-Z0-9]{6}$");
    if (!valid.match(code).hasMatch()) { setStatus(tr("국내 종목 코드는 영문 또는 숫자 6자리로 입력해 주세요.")); return; }
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_manualSocketChange = true;
        m_socket.abort();
    }
    m_reconnectTimer.stop();
    m_symbol = code;
    m_stockName.clear(); m_price.clear(); m_tradeTime.clear();
    saveSettings();
    emit changed();
    ensureToken([this](const QString &token) {
        setStatus(tr("최근 체결과 종목 정보를 불러오는 중…"));
        get("/api/v1/trades", QString("symbol=%1&count=1").arg(m_symbol), token,
            [this](QNetworkReply *reply) {
                if (reply->error() == QNetworkReply::NoError) {
                    const auto rows = QJsonDocument::fromJson(reply->readAll()).object().value("result").toArray();
                    if (!rows.isEmpty()) {
                        const auto trade = rows.first().toObject();
                        m_price = trade.value("price").toString();
                        m_tradeTime = displayTime(trade.value("timestamp").toString());
                        emit changed();
                    }
                } else setStatus(tr("초기 시세 조회 실패: %1").arg(readableError(reply)));
                reply->deleteLater();
            });
        get("/api/v1/stocks", QString("symbols=%1").arg(m_symbol), token,
            [this](QNetworkReply *reply) {
                if (reply->error() == QNetworkReply::NoError) {
                    const auto rows = QJsonDocument::fromJson(reply->readAll()).object().value("result").toArray();
                    if (!rows.isEmpty()) m_stockName = rows.first().toObject().value("name").toString();
                    emit changed();
                }
                reply->deleteLater();
            });
        connectPriceSocket(token);
    });
}

void Backend::connectPriceSocket(const QString &token) {
    QNetworkRequest request(socketUrl);
    request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
    m_socket.open(request);
}

void Backend::refreshPrice() {
    const QString code = m_symbol;
    if (!code.isEmpty()) watchSymbol(code);
}

void Backend::submitOrder(const QString &side, const QString &orderType,
                          const QString &quantity, const QString &price,
                          bool confirmHighValue) {
    if (m_selectedAccount.isEmpty()) { m_orderStatus = tr("주문 계좌를 선택해 주세요."); emit changed(); return; }
    if (side != "BUY" && side != "SELL") return;
    if (orderType != "LIMIT" && orderType != "MARKET") return;
    bool quantityOk = false;
    const double shares = quantity.toDouble(&quantityOk);
    if (!quantityOk || shares <= 0 || shares != qFloor(shares)) {
        m_orderStatus = tr("수량은 1 이상의 정수로 입력해 주세요."); emit changed(); return;
    }
    bool priceOk = false;
    const double limitPrice = price.toDouble(&priceOk);
    if (orderType == "LIMIT" && (!priceOk || limitPrice <= 0)) {
        m_orderStatus = tr("지정가를 올바르게 입력해 주세요."); emit changed(); return;
    }
    if (orderType == "MARKET" && !price.trimmed().isEmpty()) {
        m_orderStatus = tr("시장가 주문에는 가격을 지정하지 않습니다."); emit changed(); return;
    }
    const double marketPrice = m_price.toDouble();
    if (orderType == "MARKET" && marketPrice <= 0) {
        m_orderStatus = tr("시장가 주문 전 현재 시세를 받아야 합니다."); emit changed(); return;
    }
    const double estimated = shares * (orderType == "LIMIT" ? limitPrice : marketPrice);
    if (estimated >= 3000000000.0) { m_orderStatus = tr("30억 원 이상 주문은 전송할 수 없습니다."); emit changed(); return; }
    if (estimated >= 100000000.0 && !confirmHighValue) {
        m_orderStatus = tr("1억 원 이상 주문은 추가 확인이 필요합니다."); emit changed(); return;
    }
    if (m_submitting) return;
    m_submitting = true; m_orderStatus = tr("주문을 전송하는 중…"); emit changed();
    ensureToken([this, side, orderType, quantity, price, confirmHighValue](const QString &token) {
        QJsonObject body;
        body.insert("symbol", m_symbol);
        body.insert("side", side);
        body.insert("orderType", orderType);
        body.insert("quantity", quantity);
        body.insert("timeInForce", "DAY");
        body.insert("clientOrderId", QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-'));
        if (orderType == "LIMIT") body.insert("price", price);
        if (confirmHighValue) body.insert("confirmHighValueOrder", true);
        auto request = authorizedRequest(apiBase.resolved(QUrl("/api/v1/orders")), token);
        request.setRawHeader("X-Tossinvest-Account", m_selectedAccount.toUtf8());
        auto *reply = m_network.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            m_submitting = false;
            if (reply->error() != QNetworkReply::NoError) m_orderStatus = tr("주문 실패: %1").arg(readableError(reply));
            else {
                const auto result = QJsonDocument::fromJson(reply->readAll()).object().value("result").toObject();
                m_orderStatus = tr("주문 접수됨 · 번호 %1 · 체결 여부는 주문 내역에서 확인하세요.").arg(result.value("orderId").toString());
            }
            emit changed(); reply->deleteLater();
        });
    });
}

void Backend::setStatus(const QString &status) { m_status = status; emit changed(); }
