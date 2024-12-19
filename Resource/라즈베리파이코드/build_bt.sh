#sudo apt-get install libbluetooth-dev
gcc iot_client_bluetooth.c -o iot_client_bluetooth -lbluetooth -pthread
gcc iot_client_bluetooth2.c -o iot_client_bluetooth2 -lbluetooth -pthread
