🧩 1. Verify Zephyr environment inside VS Code

Paste this into Copilot inside VS Code:

Prompt:

    Check my Zephyr workspace. Verify that west, the toolchain, and the Python environment are correctly detected. Tell me if anything is missing.

🧩 2. Build a minimal Zephyr app

Prompt:

    Generate a minimal Zephyr application that prints “Hello from Zephyr BLE app” every second. Include the correct CMakeLists.txt and prj.conf for native_sim.

Then run:

Ctrl + Shift + P → CMake: Configure
Ctrl + Shift + P → CMake: Build
🧩 3. Add BLE support

Prompt:

    Add basic BLE support to this Zephyr app. Enable Bluetooth, start the Bluetooth stack, and advertise a simple device name. Update prj.conf and main.c.

🧩 4. Add a custom GATT service

Prompt:

    Add a custom GATT service with one readable characteristic and one writable characteristic. Use a 128‑bit UUID. Show me the updated code and configs.

🧩 5. Add notifications

Prompt:

    Add a notifiable characteristic to the custom GATT service. Implement a timer that sends a notification every second with a counter value.

🧩 6. Test on native_sim

Prompt:

    Show me how to run this BLE app on native_sim and how to connect to it using btmon or bluetoothctl on Linux.

🧩 7. Add hardware support (optional)

If you later want to run on real hardware:

Prompt:

    Help me switch this project from native_sim to my real board. Update the board setting, prj.conf, and any required drivers.

🧩 8. Add logging, debugging, and optimization

Prompt:

    Improve logging and debugging for this Zephyr BLE app. Enable log levels, add useful debug prints, and suggest optimizations.

🧩 9. Add a real feature (your choice)

Examples:

    Sensor reading

    Button → BLE notification

    UART → BLE bridge

    BLE OTA DFU

    BLE pairing + security

Prompt:

    Add a feature where pressing a button sends a BLE notification. Update the code and configs.

Or:

Prompt:

    Add a feature where the device reads a sensor value and exposes it over BLE as a characteristic.

🧩 10. Ask Copilot for code reviews

Prompt:

    Review my current Zephyr BLE code. Suggest improvements, fixes, and best practices.

🎯 Final recommendation

Start with this exact message inside VS Code:

Prompt:

    I want to continue building my Zephyr BLE application. First, verify my environment and confirm that CMake, west, and Python are correctly configured. Then guide me step‑by‑step through building a minimal BLE app.
