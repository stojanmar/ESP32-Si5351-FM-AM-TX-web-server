# ESP32-Si5351-FM-AM-TX-web-server
This is an ongoing project, so updates will follow.
ESP32 with Si5351 and web server to build a low power Transmitter with AM and FM.
This project allows to build an low power Transmitter, actually a FVO with AM and FM modulation. First inspiration came from the need for an RF signal generator to be used in home laboratory with HAM radios.
Second inspiration I got from excelent work of Bitluni in his GitHub repositories and videos, particularly about how to program ULP (Ultra Low Power Processor in ESP32). Others too, like Random Nerd Tutorials....
The bigest chalange was how to fast sample internal ADC and apply gain and put it in internal DAC. I could do it only with ULP. Tried also with I2S for audio stream, but didnt succeed. Maybe someone will upgrade the code with this. I2S microphones operate with much lower noise.

This project will allow you to experiment with radios and tune them properly. For conveniant usage it can be placed at any corner in the lab or even other house places where the WiFi is provided.
Since it includes teh Web server all the controll can be managed by connecting smart phones to its IP adress, simply using the ordinarry browser.
To use it in Arduino IDE you have to replace the Si5351Arduino library with my Si5351-with-FM library which will provide also the FM modulation on popular Si5351.
Set your credentials for WiFi and static IP and it should compile.

Features:
Set remotelly : Frequency, Frequency step, Modulation type, Modulation gain, Step up, Step down, Bias for RF transistors in PA ......
To operate in FM mode you dont need to build any RF output, while for the AM you need to do it or connect pin 25 to the pin Vout on Si5351.
Version ulp4 is initial, but limited to depth of modulation. Version ulp5a allows much wider modulation. Improoved versions may follow.

Connections: Audio input is on pin 34  , provide resistor devider for about 1,5V bias, and use a capacitor in serie to audio output device.
             Audio output for driving the AM in PA stage is on pin 25 (ESP32 internal DAC), connect this to RF transistor gate or base to do AM modulation.
             DAC out on pin 26 is for adjasting analog output to regulate RF transistor power.
             Si5351 out is on CLK1, standard esp32 pins for I2C data and clock pins 21 is DATA goes to SDA and 22 is CLK goes to SCL.
             
When using this for some increased power radio transmitters, you have to respect Communication and EMC regulatives for your country and internationally!
