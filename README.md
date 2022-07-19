# es-iot-led

This is the basic example of a actuator controlled by Estudio Sustenta's broker.

## Set up

### Before compiling in arduino IDE:

1. Update [libraries](https://github.com/roy-mdr/es-iot-libs)
1. Change the client id in line 10
1. Set the correct server at line 38
1. You could change the pins in line 59

### On server:

1. Configure devices and controllers databases

## To Do

- [ ] Add HTTPS
- [ ] OTA updates para firmware (diseñar para device targeted)
- [ ] ESP-NOW para crear mesh de dispositivos y sólo un cerebro conectado al broker
- [ ] ping pong support on every device
- [x] Parse payload with ArduinoJSON
- [x] Migrate to `nopoll` library [(to-do)](https://github.com/roy-mdr/es-iot-libs#to-dos)
    - [ ] WebSockets Port en broker
    - [ ] Add HTTPS to broker
    - [ ] Firmar payload como autenticacion para broker (a demás de API key)
- [ ] Migrate to `async-timer` library [(to-do)](https://github.com/roy-mdr/es-iot-libs#to-dos)