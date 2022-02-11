# airsensor2mqtt

Simple program to post air-quality measurements of an Air-Sensor USB-stick to mqtt under Linux.

You can post your USB-Stick data to mqtt once per start or start as deamon and push periodically.

# compile

you need gengetopts for generating the config-file and cmd line parser and then libmosquitto-dev and libusb-1.0-0-dev to complie the program

```
make clean
make airsensor2mqtt
```

# usage

```
Usage: airsensor2mqtt [OPTION]...
aisensor2mqtt will read VOC values from a connected VOC Stick (REHAU ans
similar) and will post that data to a mqtt server.

By Markus Reschka <markus@reschka.com>

  -h, --help                    Print help and exit
  -V, --version                 Print version and exit
  -d, --debug                   Turn debug output on  (default=off)
  -v, --verbose                 Turn on output of data to stdout  (default=off)
  -t, --wait-time=INT           Number of seconds between polling data from
                                  sensor  (default=`10')
  -1, --one-read                Read One value and then exit  (default=off)
  -s, --save-config=STRING      Take all the options from here and save to the
                                  given config file
  -l, --load-config=STRING      Load options from file
  -m, --mqtt-server=STRING      mqtt broker address  (default=`127.0.0.1')
  -o, --mqtt-server-port=portnumber
                                mqtt broker port  (default=`1883')
  -p, --mqtt-password=STRING    mqtt broker password
  -u, --mqtt-username=STRING    mqtt broker username
  -q, --mqtt-qos-setting=INT    mqtt qos setting  (default=`0')
  -r, --mqtt-no-retain          disable mqtt retain  (default=off)
  -b, --mqtt-base-name=STRING   base topic for mqtt  (default=`voc')
  -n, --sensor-name=STRING      sensor name. used as mqtt client id and as name
                                  for this sensor for connected topic
                                  (default=`voc_1')

```

# authors

## airsensor2mqtt.c
Markus Reschka https://github.com/mreschka/airsensor2mqtt

## Idea from:
        Rodric Yates http://code.google.com/p/airsensor-linux-usb/
        Ap15e (MiOS) http://wiki.micasaverde.com/index.php/CO2_Sensor
        Sebastian Sjoholm, sebastian.sjoholm@gmail.com
        FHEM Module CO2 https://svn.fhem.de/trac/browser/trunk/fhem/FHEM/38_CO20.pm
