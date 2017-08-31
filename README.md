# Arduino_Data_Monitor-HTTP-
An Arduino Uno webserver featuring DDNS via DUCKDNS. It monitors the mains electric for blackouts, temperature, and humidity. Clients can GET the HTTP server to get sensor values, readings, or statisitics. Additional features include remote reset, remote RTC time setup, in addition to other crafted landing pages. Multiport support. Increased security by disabling / landing page and using a not so well-known port. Enjoy.

<hr>
<p align="center">
  <img src="https://github.com/datguy-dev/Arduino_Data_Monitor-HTTP-/blob/master/vGmDDx7.jpg" title="Main Window">
</p>
<hr>

Adjust the following line of code with your DuckDNS domain and API Token:
```
sprintf(conkat,"update?domains=[DUCK DOMAIN]&token=[DUCK API TOKEN]&ip=%s", publicIP);
```
* [Link to Adafruit DS1307(RTC) & DHT11 libraries](https://github.com/adafruit)
* [Link to Ethercard(ENC28J60) library](https://github.com/jcw/ethercard)
