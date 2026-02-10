# Bitchat Bridge

The Bitchat Bridge enables communication between the [Bitchat](https://bitchat.app) Android app and MeshCore mesh network. Messages sent to the `#mesh` channel in Bitchat are relayed to MeshCore nodes, and vice versa.

## Overview

- **Bridge Direction**: Bidirectional - messages flow both ways between Bitchat and MeshCore
- **Channel**: Only the `#mesh` channel is bridged (hardcoded)
- **Identification**: Messages from Bitchat users appear with a phone emoji prefix on MeshCore nodes
- **Platforms**: ESP32 (full support) and NRF52 (limited - short messages only)

## Supported Boards

### ESP32-based (19 targets) - Full Support

ESP32 boards support long/compressed messages via ROM-based miniz decompression.

| Board | Target | MCU | Tested |
|-------|--------|-----|--------|
| Heltec LoRa32 V2 | `Heltec_v2_companion_radio_usb_bitchat` | ESP32 | |
| Heltec LoRa32 V3 | `Heltec_v3_companion_radio_usb_bitchat` | ESP32-S3 | |
| Heltec WSL3 | `Heltec_WSL3_companion_radio_usb_bitchat` | ESP32-S3 | ✓ |
| Heltec LoRa32 V4 | `heltec_v4_companion_radio_usb_bitchat` | ESP32-S3 | |
| Heltec CT62 | `Heltec_ct62_companion_radio_usb_bitchat` | ESP32-C3 | |
| Heltec Tracker V2 | `heltec_tracker_v2_companion_radio_usb_bitchat` | ESP32 | |
| LilyGo T3-S3 SX1262 | `LilyGo_T3S3_sx1262_companion_radio_usb_bitchat` | ESP32-S3 | |
| LilyGo T3-S3 SX1276 | `LilyGo_T3S3_sx1276_companion_radio_usb_bitchat` | ESP32-S3 | |
| LilyGo T-Deck | `LilyGo_TDeck_companion_radio_usb_bitchat` | ESP32-S3 | |
| LilyGo T-Lora V2.1 | `LilyGo_TLora_V2_1_1_6_companion_radio_usb_bitchat` | ESP32 | ✓ |
| Station G2 | `Station_G2_companion_radio_usb_bitchat` | ESP32 | |
| Seeed Xiao C3 | `Xiao_C3_companion_radio_usb_bitchat` | ESP32-C3 | |
| Seeed Xiao S3 WIO | `Xiao_S3_WIO_companion_radio_usb_bitchat` | ESP32-S3 | |
| Ebyte EoRa-S3 | `Ebyte_EoRa-S3_companion_radio_usb_bitchat` | ESP32-S3 | |
| Meshadventurer SX1262 | `Meshadventurer_sx1262_companion_radio_usb_bitchat` | ESP32 | |
| Meshadventurer SX1268 | `Meshadventurer_sx1268_companion_radio_usb_bitchat` | ESP32 | |
| ThinkNode M2 | `ThinkNode_M2_companion_radio_usb_bitchat` | ESP32-S3 | |
| ThinkNode M5 | `ThinkNode_M5_companion_radio_usb_bitchat` | ESP32-S3 | |

### NRF52-based (18 targets) - Limited Support

NRF52 boards do **not** support long/compressed messages due to heap constraints (SoftDevice BLE reserves significant memory). Short messages (<256 bytes uncompressed) work fine.

| Board | Target | Tested |
|-------|--------|--------|
| Wio Tracker L1 Pro | `WioTrackerL1_companion_radio_usb_bitchat` | ✓ |
| RAK 4631 | `RAK_4631_companion_radio_usb_bitchat` | |
| RAK WisMesh Tag | `RAK_WisMesh_Tag_companion_radio_usb_bitchat` | |
| Seeed Xiao NRF52 | `Xiao_nrf52_companion_radio_usb_bitchat` | |
| LilyGo T-Echo | `LilyGo_T-Echo_companion_radio_usb_bitchat` | |
| Heltec Mesh Solar | `Heltec_mesh_solar_companion_radio_usb_bitchat` | |
| Heltec T114 | `Heltec_t114_companion_radio_usb_bitchat` | |
| Heltec T114 (no display) | `Heltec_t114_without_display_companion_radio_usb_bitchat` | |
| Keepteen LT1 | `KeepteenLT1_companion_radio_usb_bitchat` | |
| Mesh Pocket | `Mesh_pocket_companion_radio_usb_bitchat` | |
| Pro Micro | `ProMicro_companion_radio_usb_bitchat` | |
| SenseCap Solar | `SenseCap_Solar_companion_radio_usb_bitchat` | |
| ThinkNode M1 | `ThinkNode_M1_companion_radio_usb_bitchat` | |
| Nano G2 Ultra | `Nano_G2_Ultra_companion_radio_usb_bitchat` | |
| Ikoka Handheld | `ikoka_handheld_nrf_e22_30dbm_companion_radio_usb_bitchat` | |
| Ikoka Stick | `ikoka_stick_nrf_30dbm_companion_radio_usb_bitchat` | |
| Ikoka Nano | `ikoka_nano_nrf_30dbm_companion_radio_usb_bitchat` | |
| Minewsemi ME25LS01 | `Minewsemi_me25ls01_companion_radio_usb_bitchat` | |
| T1000-E | `t1000e_companion_radio_usb_bitchat` | |

### Long Message Support

| Platform | Long Messages | Reason |
|----------|---------------|--------|
| ESP32/S3/C3 | ✓ Yes | ROM-based miniz (no heap usage) |
| NRF52 | ✗ No | Heap too small for miniz + SoftDevice BLE |

**Recommendation**: Use ESP32-based boards for full Bitchat functionality.

## Build Targets

### USB + Bitchat (`*_companion_radio_usb_bitchat`)

This is the recommended configuration:

- **MeshCore companion app**: Connects via USB serial
- **Bitchat app**: Connects via standalone BLE

This gives the best experience because:
- The MeshCore web app works reliably via USB serial
- Bitchat has dedicated BLE access without sharing with MeshCore

### Building

```bash
# Build for Heltec Wireless Stick Lite V3
pio run -e Heltec_WSL3_companion_radio_usb_bitchat

# Flash
pio run -e Heltec_WSL3_companion_radio_usb_bitchat -t upload
```


## Troubleshooting

### I don't see the peer in my Bitchat app

You probably used the same node with Meshcore Companion BLE node and
your phone still connects to it. Unpair it in bluetooth settings.
Bitchat does not require, nor supports pairing.

## How It Works

### Channel Bridging

1. **Bitchat to MeshCore**: When a Bitchat user sends a message to `#mesh`, the bridge:
   - Receives the message via BLE
   - Decompresses it (Bitchat uses zlib compression)
   - Extracts the sender nickname and message content
   - Sends it to the MeshCore #mesh with a phone emoji prefix

2. **MeshCore to Bitchat**: When a MeshCore node sends a message to the `#mesh` channel, the bridge:
   - Receives the mesh packet
   - Creates a Bitchat MESSAGE packet with the sender name and content
   - Broadcasts it via BLE

### Message Format

Messages from Bitchat appear on MeshCore as:
```
<sender>: message content
```

Messages from MeshCore appear on Bitchat with the MeshCore sender's name in angle brackets.

### Long Messages

MeshCore packets have a ~127-byte payload limit. Long Bitchat messages are automatically split into multiple parts with `[1/N]` indicators.

## Limitations

1. **Only #mesh channel**: Only the `#mesh` hashtag channel is bridged. DMs and other channels are ignored.

2. **Message size**: MeshCore packets are limited to ~127 bytes. Long messages are split into multiple parts.

3. **No end-to-end encryption bridge**: Bitchat's Noise protocol encryption and MeshCore's encryption are separate. Messages are decrypted/re-encrypted at the bridge.

4. **No file/image transfer**: Bitchat file transfers (images, etc.) are not supported on the mesh.

5. **Time synchronization**: The bridge synchronizes its clock from incoming Bitchat packets. If no Bitchat client connects, timestamps may be inaccurate. Time needs to be accurate for Bitchat to work.

6. **NRF52 long message limitation**: NRF52 devices cannot decompress long/compressed Bitchat messages due to heap constraints. Messages >256 bytes (before compression) may fail on NRF52.

### Can I run this is a MeshCore repeater?

No, it can only work as companion node, not a repeater. Unfortunately these nodes don't have enough RAM to do multiple jobs (MeshCore repeater + bitchat repeater). So you have to pick either-or. Anyway, it is also because you want it placed differently. MeshCore repeater should be on a hill, or on a roof of a very high building. If you put it somewhere close to people, you are basically jamming the spectrum and not helping the network. 

On the other hand, the bitchat repeater should be where people are (Bluetooth needs to be really close)- in an office building, apartment, public square, close to ground. So you don't want to run both on the same device anyway.

## Debugging

To enable debug output, use one of these methods:

### Method 1: Environment variable (temporary)

```bash
# Build with debug output enabled
PLATFORMIO_BUILD_FLAGS="-D BITCHAT_DEBUG=1" pio run -e Heltec_WSL3_companion_radio_usb_bitchat
```

### Method 2: Edit platformio.ini (persistent)

Add `-D BITCHAT_DEBUG=1` to your environment's build_flags:

```ini
[env:Heltec_WSL3_companion_radio_usb_bitchat]
build_flags =
  ${Heltec_WSL3_companion_radio_usb.build_flags}
  ${bitchat_base.build_flags}
  -D BITCHAT_DEBUG=1   ; Enable debug output
```

This enables detailed logging of:
- Message parsing
- Relay confirmations
- Peer cache updates
- Fragment reassembly

## Technical Details

### BLE Service

- **Service UUID**: `F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5C`
- **Characteristic**: Single read/write/notify characteristic
- **MTU**: 517 bytes (max BLE MTU for large messages)

### Channel Key

The `#mesh` channel uses a hashtag-derived key:
```
SHA256("#mesh")[0:16] = 5b664cde0b08b220612113db980650f3
```

This matches MeshCore's hashtag room key derivation.

### Protocol Support

- Message decompression (zlib via ESP32 ROM miniz, ESP32 only)
- Ed25519 message signing
- Peer announcement/discovery
- REQUEST_SYNC handling (sends cached messages)
- Fragment reassembly for long messages

## Release Builds

To build all bitchat targets for a release:

```bash
export FIRMWARE_VERSION=v0.2-bitchat

# Build individual targets
sh build.sh build-firmware Heltec_WSL3_companion_radio_usb_bitchat

# Or build all targets manually
for target in \
  Heltec_v2_companion_radio_usb_bitchat \
  Heltec_v3_companion_radio_usb_bitchat \
  Heltec_WSL3_companion_radio_usb_bitchat \
  # ... add all targets
do
  sh build.sh build-firmware $target
done
```

Output files:
- ESP32: `.bin` + `-merged.bin` files
- NRF52: `.uf2` + `.zip` files
