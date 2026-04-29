# TTN + Telegram Setup Guide

Quick guide for setting up emergency blackout notifications using The Things Network (TTN) and Telegram Bot.

## Overview

**Alert Flow:**

```
Gateway (LoRaWAN) → TTN Cloud → Telegram Bot API → Your Phone
```

**Why this works during power blackout:**

- Gateway has battery backup (30-60 minutes)
- TTN and Telegram are cloud services (always online)
- Works when Home Assistant is down

---

## Step 1: Create Telegram Bot (5 minutes)

1. Open Telegram app
2. Search for **@BotFather**
3. Send command: `/newbot`
4. Follow prompts:
   - Bot name: "Home Security Alert Bot" (or any name)
   - Username: `my_home_security_bot` (must end with 'bot')
5. **Save the bot token** (format: `123456789:ABCdefGHIjklMNOpqrsTUVwxyz`)
6. Start a chat with your bot:
   - Click the link from BotFather
   - Send `/start` to your bot

### Get Your Chat ID

7. Search for **@userinfobot** in Telegram
8. Start chat with @userinfobot
9. It will reply with your **chat ID** (a number like `123456789`)
10. **Save this chat ID**

---

## Step 2: Create TTN Application (10 minutes)

1. Go to https://console.cloud.thethings.network/
2. Sign in or create account
3. Click **"Create application"**
4. Fill in:
   - **Application ID**: `smart-home-gateway` (or any unique name)
   - **Application name**: "Smart Home Security"
5. Click **"Create application"**

### Generate API Key (Optional - for MQTT access later)

6. Go to **API keys** tab
7. Click **"Add API key"**
8. Name: "Home Assistant MQTT"
9. Rights: Select "Read application traffic (uplink and downlink)"
10. Click **"Create API key"**
11. **Copy and save the key** (starts with `NNSXS.`)

---

## Step 3: Register LoRaWAN Device (10 minutes)

1. In your TTN application, go to **"End devices"**
2. Click **"Register end device"**
3. Select **"Manually"**
4. Fill in:
   - **Frequency plan**: Europe 863-870 MHz (SF9 for RX2 - recommended)
   - **LoRaWAN version**: MAC V1.0.3
   - **Regional Parameters version**: RP001 Regional Parameters 1.0.3 revision A
   - **JoinEUI (AppEUI)**: Click "Fill with zeros" or use your own
   - **DevEUI**: Click "Generate" or use your LoRa module's hardware EUI
   - **AppKey**: Click "Generate"
   - **End device ID**: `gateway-01`

5. Click **"Register end device"**
6. **Save these credentials** - you'll need them in your gateway code:
   - DevEUI (16 hex characters)
   - AppEUI (16 hex characters)
   - AppKey (32 hex characters)

---

## Step 4: Add Payload Decoder (5 minutes)

1. In your device, go to **"Payload formatters"** tab
2. Select **"Uplink"**
3. **Formatter type**: Custom JavaScript formatter
4. Paste this code:

```javascript
function decodeUplink(input) {
	var decoded = {
		messageType: input.bytes[0],
		timestamp:
			(input.bytes[1] << 24) |
			(input.bytes[2] << 16) |
			(input.bytes[3] << 8) |
			input.bytes[4],
		battery: input.bytes[5],
	};

	var typeNames = {
		0x01: "power_blackout",
		0x02: "power_restored",
	};

	decoded.type = typeNames[decoded.messageType] || "unknown";

	return {
		data: decoded,
	};
}
```

5. Click **"Save changes"**

---

## Step 5: Configure Telegram Webhook (5 minutes)

### Simple Method (Static Message)

1. In your application, go to **"Integrations"** → **"Webhooks"**
2. Click **"Add webhook"**
3. Select **"Custom webhook"**
4. Fill in:
   - **Webhook ID**: `telegram-alert`
   - **Webhook format**: JSON
   - **Base URL**:
     ```
     https://api.telegram.org/bot<YOUR_BOT_TOKEN>/sendMessage?chat_id=<YOUR_CHAT_ID>&text=🚨%20Power%20Blackout%20Detected!&parse_mode=Markdown
     ```
     _(Replace `<YOUR_BOT_TOKEN>` and `<YOUR_CHAT_ID>` with your actual values)_
5. **Enable message types**:
   - ✓ Uplink message
   - Leave "Uplink message path" empty
6. Click **"Add webhook"**

### Testing the Webhook

1. Go to your device in TTN Console
2. Go to **"Messaging"** tab
3. Look for any uplink messages (or trigger one from your gateway)
4. You should receive a Telegram notification!

If no device yet, you can test by simulating an uplink later when the gateway is ready.

---

## Step 6: Gateway Firmware Setup

### Update Your Gateway Code

1. Open your gateway project
2. Add the LoRaWAN adapter library
3. Update `platformio.ini`:

```ini
lib_extra_dirs = ../Adapter_Lib
```

4. In your `main.cpp`, configure credentials:

```cpp
#include <LoRaWAN_Adapter.h>

// LoRa module pins
#define RST_PIN 23
#define RX_PIN 18
#define TX_PIN 19

// Your TTN credentials from Step 3
const char* DEV_EUI = "YOUR_DEV_EUI_HERE";
const char* APP_EUI = "YOUR_APP_EUI_HERE";
const char* APP_KEY = "YOUR_APP_KEY_HERE";

HardwareSerial loraSerial(2);
LoRaWAN myLoRaWAN(RST_PIN, RX_PIN, TX_PIN, loraSerial, DEV_EUI, APP_EUI, APP_KEY);

void setup() {
    Serial.begin(115200);

    // Initialize and join TTN
    if (!myLoRaWAN.init()) {
        Serial.println("LoRaWAN init failed!");
        return;
    }

    if (!myLoRaWAN.join()) {
        Serial.println("TTN join failed!");
        return;
    }

    Serial.println("Joined TTN successfully!");
}

void loop() {
    // When power blackout detected:
    if (powerBlackoutDetected) {
        uint8_t battery = readBatteryLevel();
        myLoRaWAN.sendBlackoutAlert(battery);
    }
}
```

5. Upload firmware to gateway
6. Monitor serial output for join status
7. Trigger a test alert and verify Telegram notification

---

## Verification Checklist

- [ ] Telegram bot created and bot token saved
- [ ] Chat ID retrieved and saved
- [ ] TTN application created
- [ ] LoRaWAN device registered with OTAA credentials
- [ ] Payload decoder configured in TTN
- [ ] Telegram webhook configured and tested
- [ ] Gateway firmware updated with credentials
- [ ] Gateway successfully joins TTN network
- [ ] Test alert triggers Telegram notification

---

## Troubleshooting

### Telegram notification not received

1. **Check webhook URL**: Ensure bot token and chat ID are correct
2. **Test manually**: Open this URL in browser:
   ```
   https://api.telegram.org/bot<YOUR_BOT_TOKEN>/sendMessage?chat_id=<YOUR_CHAT_ID>&text=Test
   ```
3. **Check webhook logs**: In TTN Console → Integrations → Your webhook → Request log

### Gateway won't join TTN

1. **Check credentials**: Verify DevEUI, AppEUI, AppKey match TTN Console
2. **Check coverage**: Visit https://www.thethingsnetwork.org/map
3. **Check frequency**: Ensure module is configured for EU868
4. **Wait longer**: OTAA join can take 10-30 seconds
5. **Check serial output**: Look for error messages

### Payload decoder not working

1. Verify JavaScript syntax in TTN Console
2. Check "Live data" tab in TTN for decode errors
3. Test decoder with sample payload in TTN Console

### Webhook shows errors

- **400 Bad Request**: Check chat_id format (should be number, not @username)
- **401 Unauthorized**: Bot token is incorrect
- **404 Not Found**: Check URL format and bot token

---

## Advanced: Dynamic Payload in Webhook (Optional)

If you want battery level and timestamp in the notification, you'll need a middleware service (Pipedream, Cloudflare Worker, etc.) to transform the TTN payload format to Telegram's format.

This is not needed for the basic blackout alert, but useful for future enhancements.

---

## Security Notes

- **Never commit credentials**: Keep bot token, API keys, and AppKey secret
- **Add to .gitignore**: Exclude any files with credentials
- **Use environment variables**: For production deployments
- **Rotate keys**: If compromised, regenerate in TTN/Telegram

---

## Multiple Users (Family Members)

**Option 1: Telegram Group (Recommended)**

1. Create a Telegram group
2. Add your bot to the group
3. Get group chat ID (it will be negative: `-123456789`)
4. Use group chat ID in webhook URL

**Option 2: Multiple Webhooks**

- Create separate webhook for each user
- Each with their own chat_id

---

## Next Steps

- [ ] Test end-to-end with Home Assistant powered OFF
- [ ] Measure actual battery duration during test
- [ ] Add power monitoring circuit to gateway
- [ ] Implement automatic power restoration notification
- [ ] (Optional) Add Home Assistant MQTT integration for historical logging

---

## Useful Links

- TTN Console: https://console.cloud.thethings.network/
- TTN Coverage Map: https://www.thethingsnetwork.org/map
- TTN Documentation: https://www.thethingsindustries.com/docs/
- Telegram Bot API: https://core.telegram.org/bots/api
- Adapter Library Documentation: `Adapter_Lib/docs/Library_User_Guide.md`
