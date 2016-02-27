														Programming Assignment 2 (CSE 589)
							

The Programme is a simulation of distance vector routing protocol among 5 servers listed as below:
1: timberlake.cse.buffalo.edu
2: highgate.cse.buffalo.edu
3: underground.cse.buffalo.edu
4: embankment.cse.buffalo.edu
5: euston.cse.buffalo.edu	

Each server is provided a Topology file consisting of the required data. /* CODE LINE 77 */
				
							
The Programme execution starts as follows :
1) 	The first command which is a command line argument is -server -t <topology-file> -i <routing-table-interval>
	It reads the topology file and creates the initial Routing table.
	
2)	It follows a list of command at any given point of time in the code. The description of the command is as follows:
		
		2.1) 	-update <serverID1> <serverID2> <Link Cost(Not Infinite)> /* CODE LINE 454 */
				This command updates the given link cost to the specified link cost. Here we need to see that one of the 
				servers is the Self router and other one is it's neighbour.
		
		2.2)	-update <serverID1> <serverID2> <Link Cost(Infinite)>	/* CODE LINE 454 */	
				Here we need to see that one of the servers is the Self router and other one is it's neighbour. The cost is 
				set to infinite in the routing table and the neighbour entry is removed as there is no longer a link between them.
				
		2.3)	-step /* CODE LINE 804 */
				This command sends the current routing table to its neighbours immediately(One way to send updates)
				
		2.4)	-packets /* CODE LINE 812 */
				It keeps a track of the received packets from its neighbours since the last <packets> command was typed.
				
		2.5)	-display /* CODE LINE 128 */
				It displays the current routing table in the following format.
				<Destination Server ID>    <Next Hop ID>   <LinkCost>
				
		2.6)	-disable <server-ID> /* CODE LINE 299 */
				We need to check if the serverID is the neighbour. If yes, we need to disconnect the link and remove the neighbour.
				We set the cost to Infinite and remove the neighbour from the neighbour list
			
		2.7)	-crash /* CODE LINE 853 */
				This command is to simulate Crash. Here we set close the main Socket to stop all actions to and from the router.
				
3)	We have set a timeout initially. This will act as an update indicator. At each timeout, routing table updats are sent to the neighbours.

4)	At three timeouts we check the number of packets received from each of the neighbours. If the count is 0, we set the distance to 0
	and the neighbour is removed from the neighbour list.

The data structures used are : 
1) For maintainig the routing table :
	line 43	:	char serverID[5]; // Server(Router) ID
	line 44 :	char nexthopID[5]; // Router ID for the next hop to reach the Router ID from the Host Router
	line 47 :	int serverDistance[5]; // Cost to server
	
2)	For maintainig update message :
	line 59 :	//For Data of Neighbours
				struct neighbour{
					short nPort; //For 2 byte Port
					unsigned long nIP; //For 4 byte IP
					short nID; //For 2 byte server ID 
					int nCost; //For 4 byte link Cost
				};

	line 67 :	//Main Message Packet Structure
				struct message{
					short updateNum; //For 2 byte number of Updates
					short sPort; //For 2 byte Port
					unsigned long sIP; //For 4 byte IP
					struct neighbour updateNode[mCount]; // Here mCount is a constant defined for the total number of routers in the Topology
				}mainNode,recNode;	//mainNode for sending, RecNode for Receiving Packets		
		
	So instantiation of <struct message> will hold the data as required. 