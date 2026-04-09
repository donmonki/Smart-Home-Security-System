# Library User Guide

### _General Description_:
The library is created to use for the Node- and Gateway development to send and receive data.\
Supported Communication protocols:
- LoRa P2P
- LoRaWAN (TBD)
- MQTT (TBD)

### _LoRaP2P Adapter User Guide_

The implemented adapter for LoRa P2P communication uses the `LoraP2P` class. When creating an object the RX, TX and Reset and the used hardware serial port need to be given in the argument.

The following functions can be used for the following use cases:
- `bool LoraP2P::moduleInit() `: LoRa module initializaton method. It is important to be called during the setup. 
    - `return`: Returns `false` immediately if any initialization commands fails. If successful `true`
- `bool LoraP2P::transmitPayload(const LoRaPayload &payload)`:
- `bool LoraP2P::receivePayload(LoRaPayload &receivedData)`: 