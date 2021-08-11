# Simul
This program does a probabilistic simulation of a client server system. The
clients generate job packages at random times and place them on the global queue. Every
package takes a random amount of time to execute and if the servers are busy the package
stays on the queue. At every time interval a the client and the server are woken up to simulate
work done during this interval.
