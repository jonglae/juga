# Juga

Juga shows live Korean stock trades from Toss Securities and can place manual buy or sell orders after a review and confirmation step.

## Setup

1. In Toss Securities WTS, open **Settings → Open API**, create a client ID and client secret, and add this computer's public IP to the allowed IP list.
2. Launch Juga from **Super + Space**, open **Settings**, and enter the credentials.
3. Select the account, enter a six-character Korean stock code such as `005930`, and press **Watch**.

Market data uses Toss Securities' authenticated WebSocket feed. The initial display uses the latest trade returned by its REST API; the price updates when new trades arrive. The feed covers combined KRX and NXT sessions. This app does not advise on investments.

Credentials are saved in `~/.config/juga/settings.ini` with owner-only file permissions. Use a limit order by default. Market orders need a separate confirmation and can execute at a different price. Every order requires a review dialog; orders of ₩100 million or more require an additional acknowledgment. The server rejects orders of ₩3 billion or more.

## Build and install

Run `bin/build`, then `bin/install` to install the Arch package and launcher entry.

## Shortcuts

| Key | Action |
| --- | --- |
| Space | Refresh the quote |
| ? | Show shortcuts |
| Q | Quit |

## License

MIT. See [LICENSE](LICENSE).
