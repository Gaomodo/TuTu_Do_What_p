import paho.mqtt.client as mqtt
import time

BROKER = "broker.emqx.io"
PORT = 1883
TOPIC = "/m1s/image"


def on_connect(client, userdata, flags, rc):
    print("Connected with result code %d" % rc)
    client.subscribe(TOPIC)


def on_message(client, userdata, msg):
    jpeg_bytes = msg.payload
    print("收到图片: %d 字节" % len(jpeg_bytes))

    filename = "photo_%d.jpg" % int(time.time())
    with open(filename, "wb") as f:
        f.write(jpeg_bytes)
    print("已保存: %s" % filename)


client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect(BROKER, PORT, 60)
client.loop_forever()
