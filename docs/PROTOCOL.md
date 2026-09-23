# ESP-NOW multiplayer protocol

Two skins talk to other clickers directly over **ESP-NOW**: `duel` (1v1 tug of
war) and `koth` (King of the Hill, up to 12 players). No router, no internet and
no pairing step are needed — devices find each other by broadcast.

Everything below matches
[`src/duel/DuelProtocol.h`](../src/duel/DuelProtocol.h) and
[`src/duel/DuelNet.cpp`](../src/duel/DuelNet.cpp).

## Transport

| Item | Value |
|---|---|
| Radio | ESP-NOW, Wi-Fi station mode |
| Channel | 1 (fixed: `esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE)`) |
| Modem sleep | disabled for reliability |
| Broadcast address | `FF:FF:FF:FF:FF:FF` (registered as a peer at startup) |
| Encryption | none — payloads are plaintext |
| Filtering | any packet whose `proto` is not `0x0D01` is dropped |
| Buffering | receive callback (Wi-Fi task) pushes into an 8-slot ring buffer; the main loop drains it via `DuelNet::poll()` |

Every device runs the same firmware, which is why the protocol lives in a single
header.

## Packet

32 bytes, no padding, sent as-is:

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 2 | `proto` | always `0x0D01`; mismatched packets are ignored |
| 2 | 1 | `op` | opcode (see below) |
| 3 | 1 | `flags` | `OP_ACCEPT`: 1 = accept, 0 = decline; `KOTH OP_STATE`: `FLAG_IS_KING = 0x01` |
| 4 | 4 | `duelId` | round identifier, unique on the challenger's side |
| 8 | 4 | `clicks` | score (`OP_STATE` / `OP_RESULT`), or a delay in `KOTH OP_READY` |
| 12 | 4 | `stamp` | sender's `millis()`, informational |
| 16 | 16 | `name` | sender nickname, NUL-terminated (`MAX_NAME = 15`) |

Duel opcodes are `1…5`, KOTH uses `10…13`, so a skin simply ignores packets that
belong to the other mode.

## Duel (`op` 1…5)

| Op | Value | Direction | Meaning |
|---|---|---|---|
| `OP_ANNOUNCE` | 1 | broadcast | "I am looking for an opponent, here is my name" |
| `OP_CHALLENGE` | 2 | unicast | challenge a specific device |
| `OP_ACCEPT` | 3 | unicast | accept (`flags = 1`) or decline (`flags = 0`) |
| `OP_STATE` | 4 | unicast | my current click count during the duel |
| `OP_RESULT` | 5 | unicast | final score after the 15 s are over |

Timings (all in `DuelProtocol.h`):

| Constant | Value | Purpose |
|---|---|---|
| `ANNOUNCE_INTERVAL_MS` | 600 | beacon period while opponents are visible |
| `ANNOUNCE_IDLE_MS` | 2000 | slower beacon when nobody is around |
| `OPPONENT_TIMEOUT_MS` | 3000 | drop an opponent after this silence |
| `CHALLENGE_TIMEOUT_MS` | 10000 | give up waiting for an answer |
| `CHALLENGE_HOLD_MS` | 1000 | click-hold that issues a challenge |
| `DUEL_DURATION_MS` | 15000 | duel length |
| `STATE_INTERVAL_MS` | 200 | score exchange period |
| `DUEL_DISCONNECT_MS` | 2000 | opponent silence after which the duel is aborted |
| `END_GRACE_MS` | 700 | short wait for the final score |
| `RESULT_SHOW_MS` | 5000 | minimum time the result stays on screen |
| `SEARCH_ACTIVE_MS` | 40000 | how long the skin searches before it may go idle |

State machine: `SEARCH → (challenge/accept) → DUEL → END_WAIT → RESULT → SEARCH`.
If both devices press "challenge" simultaneously, the one with the **larger MAC**
is the owner of the `duelId`, and both sides adopt it, so the exchange stays
consistent. If the opponent goes silent for `DUEL_DISCONNECT_MS`, the duel is
aborted **without** a winner, so the remaining player cannot click up an unfair
victory.

## King of the Hill (`op` 10…13)

| Op | Value | Direction | Meaning |
|---|---|---|---|
| `OP_ANNOUNCE` | 10 | broadcast | "I am playing" — feeds the player counter |
| `OP_READY` | 11 | broadcast | round is about to start (`duelId` = round id, `clicks` = delay) |
| `OP_GO` | 12 | broadcast | round starts now |
| `OP_STATE` | 13 | broadcast | `clicks` = current power, `stamp` = accumulated crown time, `flags` bit `FLAG_IS_KING` |

| Constant | Value |
|---|---|
| `ANNOUNCE_INTERVAL_MS` / `ANNOUNCE_IDLE_MS` | 800 / 2000 |
| `PLAYER_TIMEOUT_MS` | 3000 |
| `READY_DELAY_MIN_MS` … `READY_DELAY_MAX_MS` | 1500 … 4000 |
| `READY_TIMEOUT_MS` | 6000 (no `GO` → back to waiting) |
| `KOTH_ROUND_MS` | 60000 |
| `KOTH_TICK_MS` | 250 (power update and state exchange) |
| `POWER_WINDOW_MS` / `POWER_SLOTS` | 2000 / 8 |
| `START_HOLD_MS` | 1000 |
| `END_GRACE_MS` / `RESULT_SHOW_MS` | 700 / 4000 |
| `MAX_PLAYERS` | 12 |

Rules:

- **Power** is the number of clicks in a sliding 2 s window, kept in a ring of
  eight 250 ms slots.
- The **king** is decided locally by every device: highest power wins, ties are
  broken by the smaller MAC. Only a player who is actually clicking (power > 0)
  counts as a threat — the natural fading of your own power window never costs
  you the crown.
- Crown time accumulates while you hold the crown, and the winner is the player
  with the largest accumulated crown time.

## Nicknames

The nickname comes from NVS (`SettingsStore::loadNick()`, set on the device's
Wi-Fi page). If it is empty, the skin falls back to `Clicker-XXXX` built from the
last two MAC bytes, so devices are distinguishable out of the box.

## Security notes

- ESP-NOW here is **unencrypted** and there is no authentication: anyone in radio
  range can observe, replay or spoof packets. That is acceptable for a local toy
  game — the scores in `duelWins` / `kothWins` are cosmetic and only stored on the
  devices themselves.
- Packets are validated only by the `proto` field and by MAC matching, so a
  mismatched firmware version simply will not be seen (`PROTO` is the compatibility
  gate — bump it whenever the packet layout changes).
