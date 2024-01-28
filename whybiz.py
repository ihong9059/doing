import threading
import time 
import socket 

testHost0 = '192.168.0.200'
testPort0 = 2000

raspIp = '127.0.0.1'
raspPort = 6666

sendFlag = False
sendData = ''

class SendThread(threading.Thread):
    def __init__(self):
        print('start send thread')
        super().__init__() 

    def setFlag(self, flag, data):
        self.sendFlag = flag
        self.sendData = data
        
    def bind(self):
        mainCount = 0
        self.client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.client.connect((testHost0, testPort0))
        print('connected to whybiz')

        global sendFlag
        global sendData
        while True:
            # print("Send mainCount: ", mainCount)
            # self.client.connect((testHost0, testPort0))
            try:
                data = self.client.recv(256)
                if(sendFlag):
                    print("send data for control")
                    sendFlag = False
                    self.client.sendall(sendData)

                # data = self.client.recv(256)
                rasp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                rasp.connect((raspIp, raspPort))
                rasp.sendall(data)
                rasp.close()
                print(".")
            except Exception as e:
                print(e)
                self.client.close()
                print('Fail to send data to server:{}, \
                    port:{}'.format(testHost0, testPort0))
                time.sleep(5)
                print("Reconnect to Host: {}, port: {}", testHost0, testPort0)
                self.client.connect((testHost0, testPort0))
            mainCount += 1    

    def run(self):
        try:
            th = threading.Thread(target=self.bind)
            th.start()
        except:
            print('----------- Thread except')

myIp = '127.0.0.1'
myPort = 7777

class ReceiveThread(threading.Thread):
    def __init__(self):
        print('start Receive thread')

        super().__init__() 

    def bind(self):
        mainCount = 0
        my_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        my_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        my_socket.bind((myIp, myPort))
        # my_socket.bind((testHost0, testPort0))
        my_socket.listen()
        global sendFlag
        global sendData
        while True:
            print("Send mainCount: ", mainCount)
            myClient, addr = my_socket.accept()
            try:
                data = myClient.recv(1024)
                if(data):
                    print(data.decode())
                    sendFlag = True
                    sendData = data
                    print('sent to send thread')
                # self.sendThread.setFlag(self, True, data)
            except Exception as e:
                print(e)
                myClient.close()
                print('Fail to send date to server:{}, \
                    port:{}'.format(testHost0, testPort0))
            mainCount += 1    
            # time.sleep(3)

    def run(self):
        try:
            th = threading.Thread(target=self.bind)
            th.start()
        except:
            print('----------- Thread except')


def main():
    mainCount = 0

    mySend = SendThread()
    mySend.daemon = True
    mySend.start() 

    myReceive = ReceiveThread()
    myReceive.daemon = True
    myReceive.start() 


    while True:
        print('main: {}'.format(mainCount))
        mainCount += 1

        time.sleep(1)

if __name__ == '__main__':
    main()

