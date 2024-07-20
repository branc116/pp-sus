import random

global_id = 0

status_disconnected = 0
status_connecting = 0
status_connected = 0

class MsgExistingClient:
    def __init__(self, client):
        self.client = client

class MsgNewClient:
    def __init__(self, client):
        self.client = client

class MsgHi:
    pass

class Client:
    def __init__(self, debug_id, tracker):
        self.id = debug_id

        self.open_coms = set()
        self.msgs = []
        self.state = status_disconnected
        self.known_clients = set() 
        self.net_id = []
        self.tracker = tracker
        self.sleep_logic = 0

    def send_msg(self, msg, to):
        to.rcv_msg(msg, self)
        self.open_coms.add(to)

    def rcv_msg(self, msg, other):
        if other in open_coms:
            self.msgs += [(msg, other)]

    def thread_msg_tick(self):
        for msg, other in self.msgs:
            assert other != self
            if other == self.tracker:
                if isinstance(msg, MsgExistingClient):
                    assert self.status == status_connecting
                    self.known_clients.add(msg.client)
                if isinstance(msg, MsgNewClient):
                    self.known_clients.add(msg.client)
            else:
                assert other in open_coms

    def thread_logic_tick(self):
        if self.sleep_logic > 0:
            self.sleep_logic -= 1
            return
        if self.state == status_disconnected:
            self.send_msg(MsgHi, self.tracker)
            self.state = status_connecting
        elif self.state == status_connecting:
            if (len(self.known_clients) > 0):
                c = random.choice(self.known_clients)
                self.send_msg(MsgHi)
                self.sleep_logic = 3
        else:
            assert False

class Tracker:
    def __init__(self):
        self.clients = {}
        self.msgs = []

    def send_msg(self, msg, to):
        to.rcv_msg(msg, self)

    def rcv_msg(self, msg, other):
        self.msgs += [[msg, other]]

    def thread_msg_tick(self):
        for msg, other in self.msgs:
            if isinstance(msg, MsgHi):
                assert other not in self.clients # This should be a retranssmision just to `from` client
                self.send_msg(msg, MsgHi)
                for c in self.clients:
                    self.send_msg(MsgExistingClient(c), other)
                    self.send_msg(MsgNewClient(other), c)
                self.clients.add(other)

def main():
    tracker = Tracker()
    clients = [Client(r, tracker) for r in range(4)]
    for i in range(10):
        for c in clients:
            c.thread_logic_tick()
            c.thread_msg_tick()
        tracker.thread_msg_tick()

main()
