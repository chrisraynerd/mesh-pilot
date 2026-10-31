# Dependencies and protocol references

Mesh Pilot's application source is licensed under MIT. Dependencies retain their
own licences and notices. PlatformIO downloads them; their source is not vendored
in this repository.

- TFT_eSPI 2.5.43: https://github.com/Bodmer/TFT_eSPI (see its licence notices).
- XPT2046_Touchscreen, pinned commit in platformio.ini:
  https://github.com/PaulStoffregen/XPT2046_Touchscreen (MIT).
- NimBLE-Arduino 1.4.3: https://github.com/h2zero/NimBLE-Arduino (Apache-2.0
  and the notices for included components).
- Arduino-ESP32 / Espressif framework and bundled mbedTLS retain their respective
  licences. See https://github.com/espressif/arduino-esp32 and its third-party notices.

The application implements the MeshCore companion protocol independently.
Protocol/reference source: https://github.com/meshcore-dev/MeshCore.
No WADAMESH source is used. Mesh Pilot is an independent project and is not an
official MeshCore product.
