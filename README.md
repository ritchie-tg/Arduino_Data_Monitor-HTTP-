# Arduino_Data_Monitor-HTTP-
    An Arduino (Uno) webserver featuring DDNS via DUCKDNS. This is an device that detects and records electrical interruptions or blackouts, the room temperature and humidity, then servers this information to clients to interpret. Additional features include client & server in one device, remote software reset, remote RTC time setup, in addition to other landing pages. Although this project is rather specific, I imagine the code could be used as a how-to, template, or reference in their own projects. Enjoy.


<hr>
<p align="center">
  <img src="https://github.com/datguy-dev/Arduino_Data_Monitor-HTTP-/blob/master/vGmDDx7.jpg" title="Main Window"><br>
  Gallery: https://imgur.com/a/b9ABL
</p>
<hr>


[DuckDNS](https://www.duckdns.org/) At the least, you MUST adjust the following line of code with your DuckDNS domain and API Token:
```
sprintf(conkat,"update?domains=[DUCK DOMAIN]&token=[DUCK API TOKEN]&ip=%s", publicIP);
```
* [Link to Adafruit RTC DS1307 & DHT11 libraries](https://github.com/adafruit)
* [Link to Ethercard (ENC28J60 mini) library](https://github.com/jcw/ethercard)
