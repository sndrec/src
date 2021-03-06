#include "cbase.h"
#include <string.h>
#include "GhostOnlineEngine.h"
#include "GhostEngine.h"
#include "GhostRecord.h"
#include "SFML\include\SFML\Network.hpp"
#include <WS2tcpip.h>

#include "tier0/memdbgon.h"


GhostOnlineEngine* GhostOnlineEngine::instance = NULL;
float GhostOnlineEngine::nextTime = 0.0f;
bool GhostOnlineEngine::shouldAct = false;
bool GhostOnlineEngine::firstTime = true;
sf::UdpSocket GhostOnlineEngine::sendSock;
sf::IpAddress GhostOnlineEngine::serverIPAddress;

GhostOnlineEngine* GhostOnlineEngine::getEngine() {
	if (instance == NULL) {
		instance = new GhostOnlineEngine();
	}
	return instance;
}

static ConVar serverIP("gh_online_ip", "127.0.0.1", 
					   FCVAR_ARCHIVE | FCVAR_REPLICATED, 
					   "The IP of the server to connect to.");

static ConVar serverPort("gh_online_port", "5145", FCVAR_ARCHIVE | FCVAR_REPLICATED, "The port of the server.");

sf::Packet& operator <<(sf::Packet& packet, GhostUtils::GhostData& data) {
	return packet << data.trailLength <<  data.trailRed << data.trailGreen << data.trailBlue << data.ghostRed << data.ghostGreen << data.ghostBlue;
}

sf::Packet& operator >>(sf::Packet& packet, GhostUtils::GhostData& data) {
	return packet >> data.trailLength >> data.trailRed >> data.trailGreen >> data.trailBlue >> data.ghostRed >> data.ghostGreen >> data.ghostBlue;
}

static unsigned ReceiveThread(void *params) {
	while (GhostOnlineEngine::getEngine()->shouldAct) {
		if (!GhostOnlineEngine::getEngine()->inTransition) {
			sf::Packet pack;
			sf::IpAddress sender;
			unsigned short port;
			if ((GhostOnlineEngine::sendSock.receive(pack, sender, port) == sf::Socket::Done) && (pack.getDataSize() > 0)) {
				if (port == serverPort.GetInt()) {
					GhostOnlineEngine::getEngine()->handleEvent(&pack);
				}
			}
		}
	}
	Msg("Shutting down thread!\n");
	return 0;
}

void GhostOnlineEngine::addRun(GhostOnlineRun* run) {
	if (run) ghosts.AddToTail(run);
}


void GhostOnlineEngine::handleEvent(sf::Packet *toRead) {
	if (getEngine()->inTransition) return;
	unsigned char firstByte;
	*toRead >> firstByte;
	switch (firstByte) {
	case 0x00://connect packet
		handleConnect(toRead);
		break;
	case 0x01://runline from some other person
		handleLine(toRead);
		break;
	case 0x02://race start

		//TODO
		break;
	case 0x03://race end

		//TODO
		break;
	case 0x04://another user disconnected
		handleDisconnect(toRead);
		break;
	case 0x05://this user being kicked from server
		handleKick(toRead);
		break;
	default:
		break;
	}
}

void GhostOnlineEngine::handleKick(sf::Packet *toRead) {
	char reason[64];
	*toRead >> reason;
	Msg("Kicked from the online server! Reason: %s\n", reason);
	disconnect(false);
}

void GhostOnlineEngine::handleDisconnect(sf::Packet *toRead) {
	Msg("Received user disconnect packet!\n");
	//user disconnect (bye bye)
	char userName[64];
	*toRead >> userName;
	Msg("%s is disconnecting!\n", userName);
	GhostOnlineRun* run = getRun(userName);
	if (run) run->EndRun();
}

void GhostOnlineEngine::handleConnect(sf::Packet* toRead) {
	Msg("Received user connect packet!\n");
	//sends each player and their name, ghost color, trail color, and trail length
	//ONE BY ONE
	char playerName[64];
	*toRead >> playerName;
	Msg("%s is connecting!\n", playerName);
	if (getRun(playerName)) return;//IMPERSONATOR! (or the person is most likely reconnecting)
	GhostUtils::GhostData data;
	*toRead >> data;
	GhostOnlineRun* run = new GhostOnlineRun(playerName, data);
	run->StartRun();
	Msg("Started run for %s!\n", playerName);
	ghosts.AddToTail(run);
}

void GhostOnlineEngine::handleLine(sf::Packet* toRead) {
	if (!(toRead->getDataSize() > 0)) return;
	char playerName[64];
	char map[64];
	float vx, vy, vz, x, y, z;
	*toRead >> playerName;
	*toRead >> map;
	*toRead >> vx;
	*toRead >> vy;
	*toRead >> vz;
	*toRead >> x;
	*toRead >> y;
	*toRead >> z;
	OnlineRunLine l = GhostUtils::createLine(playerName, map, vx, vy, vz, x, y, z);
	GhostOnlineRun* run = getRun(playerName);
	if (run) {
		if (!run->IsStarted()) {
			run->StartRun();
		}
		run->updateStep(l);
	}
}

GhostOnlineRun* GhostOnlineEngine::getRun(const char* nameOfGhost) {
	size_t size = ghosts.Count();
	for (unsigned int i = 0; i < size; i++) {
		if (Q_strcmp(ghosts[i]->ghostName, nameOfGhost) == 0) {
			return ghosts[i];
		}
	}
	return NULL;
}

GhostOnlineRun* GhostOnlineEngine::getRun(GhostOnlineEntity* ent) {
	size_t size = ghosts.Count();
	for (unsigned int i = 0; i < size; i++) {
		if (ghosts[i]->ent) {
			if (ghosts[i]->ent == ent) return ghosts[i];
		}
	}
	return NULL;
}

void GhostOnlineEngine::connect() {
	if (shouldAct) {
		Msg("Already connected!\n");
		return;
	}
	if (sendSock.bind(serverPort.GetInt()) != sf::Socket::Done) {
		Msg("Failed to bind port!\n");
		return;
	}
	serverIPAddress = serverIP.GetString();
	CreateSimpleThread(ReceiveThread, NULL);
	firstTime = true;
	shouldAct = true;
	Msg("Connected and created the thread!\n");
}

ConCommand connect_con("gh_online_connect", GhostOnlineEngine::connect, "Connects to the Online server using the IP specified in\ngh_online_ip", 0);

void GhostOnlineEngine::EndRun(GhostOnlineRun* run) {
	if (run) {
		ghosts.FindAndRemove(run);
	}
}

void GhostOnlineEngine::ResetGhosts() {
	int size = ghosts.Count();
	for (int i = 0; i < size; i++) {
		GhostOnlineRun* it = ghosts[i];
		if (it) it->StartRun();
	}
	inTransition = false;
}

void GhostOnlineEngine::stopAllRuns() {
	if (!isActive()) return;
	GhostOnlineRun* it = ghosts[0];
	if(it) it->EndRun();
	stopAllRuns();
}


void GhostOnlineEngine::disconnect(bool gameShutdown) {

	if (!shouldAct) {
		Msg("Not connected to anything!\n");
		return;
	}
	shouldAct = false;
	sf::Packet pack;

	pack << "d";
	pack << GhostRecord::getGhostName();
	sendSock.send(pack, serverIPAddress, serverPort.GetInt());
	sendSock.unbind();
	nextTime = 0.0f;

	if(!gameShutdown) getEngine()->stopAllRuns();
}

static void discon() {
	GhostOnlineEngine::getEngine()->disconnect(false);
}

ConCommand disconnect_con("gh_online_disconnect", discon, "Disconnects from the Online server.", 0);

bool GhostOnlineEngine::isActive() {
	return ghosts.Count() > 0;
}


void GhostOnlineEngine::sendFirstData() {
	if (!shouldAct) return;
	sf::Packet pack;
	pack << "c" << GhostRecord::getGhostName() << gpGlobals->trailLength << gpGlobals->trailRed << gpGlobals->trailGreen << gpGlobals->trailBlue
		<< gpGlobals->ghostRed << gpGlobals->ghostGreen << gpGlobals->ghostBlue;
	sendSock.send(pack, serverIPAddress, serverPort.GetInt());
	firstTime = false;
}

void GhostOnlineEngine::sendLine(OnlineRunLine l) {
	if (!shouldAct) return;
	sf::Packet pack;
	pack << "l" << l.name << l.map << l.velX << l.velY << l.velZ << l.locX << l.locY << l.locZ;
	sendSock.send(pack, serverIPAddress, serverPort.GetInt());
}