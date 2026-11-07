#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>
#include <QWebSocket>
#include <QVariantList>

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString symbol READ symbol NOTIFY changed)
    Q_PROPERTY(QString stockName READ stockName NOTIFY changed)
    Q_PROPERTY(QString price READ price NOTIFY changed)
    Q_PROPERTY(QString tradeTime READ tradeTime NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString orderStatus READ orderStatus NOTIFY changed)
    Q_PROPERTY(QVariantList accounts READ accounts NOTIFY changed)
    Q_PROPERTY(QString selectedAccount READ selectedAccount WRITE setSelectedAccount NOTIFY changed)
    Q_PROPERTY(bool configured READ configured NOTIFY changed)
    Q_PROPERTY(bool submitting READ submitting NOTIFY changed)

public:
    explicit Backend(QObject *parent = nullptr);
    QString symbol() const { return m_symbol; }
    QString stockName() const { return m_stockName; }
    QString price() const { return m_price; }
    QString tradeTime() const { return m_tradeTime; }
    QString status() const { return m_status; }
    QString orderStatus() const { return m_orderStatus; }
    QVariantList accounts() const { return m_accounts; }
    QString selectedAccount() const { return m_selectedAccount; }
    bool configured() const { return !m_clientId.isEmpty() && !m_clientSecret.isEmpty(); }
    bool submitting() const { return m_submitting; }

    void setSelectedAccount(const QString &account);
    Q_INVOKABLE void saveCredentials(const QString &clientId, const QString &clientSecret);
    Q_INVOKABLE void loadAccounts();
    Q_INVOKABLE void watchSymbol(const QString &symbol);
    Q_INVOKABLE void submitOrder(const QString &side, const QString &orderType,
                                 const QString &quantity, const QString &price,
                                 bool confirmHighValue);
    Q_INVOKABLE void refreshPrice();

signals:
    void changed();

private:
    void loadSettings();
    void saveSettings();
    void ensureToken(std::function<void(const QString &)> ready);
    void fetchAccountList(const QString &token);
    void connectPriceSocket(const QString &token);
    void setStatus(const QString &status);
    QNetworkRequest authorizedRequest(const QUrl &url, const QString &token) const;
    void get(const QString &path, const QString &query, const QString &token,
             std::function<void(QNetworkReply *)> callback);

    QNetworkAccessManager m_network;
    QWebSocket m_socket;
    QTimer m_reconnectTimer, m_pingTimer;
    QString m_clientId, m_clientSecret, m_token;
    QString m_symbol, m_stockName, m_price, m_tradeTime;
    QString m_status, m_orderStatus, m_selectedAccount;
    QVariantList m_accounts;
    bool m_submitting = false;
    bool m_manualSocketChange = false;
    int m_reconnectDelay = 1000;
};
