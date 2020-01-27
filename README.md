This project is a part of Operating Systems course Taught in university of Tehran.
The main Idea is to implement a file transfer system which operated in both client-server mode and p2p mode to allow clients to communicated independent of server.
server broadcasts it's own TCP port via UDP connection all the time(also known as heart beat) , when a client issues a request (upload or download) there will be two possible
outcome.
1. if server is up , then client will communicate with it
2. if server is down , then client broadcasts it's request to all availabel clients and will wait for an appropiate answer.