/*
* This file is part of DOT.
*
*     DOT is free software: you can redistribute it and/or modify
*     it under the terms of the GNU General Public License as published by
*     the Free Software Foundation, either version 3 of the License, or
*     (at your option) any later version.
*
*     DOT is distributed in the hope that it will be useful,
*     but WITHOUT ANY WARRANTY; without even the implied warranty of
*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*     GNU General Public License for more details.
*
*     You should have received a copy of the GNU General Public License
*     along with DOT.  If not, see <http://www.gnu.org/licenses/>.
*
* Copyright 2011-2013 dothub.org
*/

/*
 * OVS_2_1.cpp
 *
 *  Created on: 2013-08-28
 *      Author: Arup Raton Roy (ar3roy@uwaterloo.ca)
 */

#include "OVS_2_1.h"
#include <sstream>
#include <map>
#include <fstream>
#include <iostream>
#include "GREInterface.h"


using namespace std;

OVS_2_1::OVS_2_1(CommandExecutor* commandExec)
    :AbstractSwitch(commandExec)
{
    // TODO Auto-generated constructor stub
    this->selfLogger = Global::loggerFactory->getLogger("OVS_2_1");
    LOG4CXX_DEBUG((*selfLogger), "Constructor is called");
}


OVS_2_1::~OVS_2_1() {
    // TODO Auto-generated destructor stub
}

void OVS_2_1::clearSwitch(Switch* theSwitch) {

    ostringstream command;
    cout << theSwitch->getName() << ":" << theSwitch->getIPOfMachine() << endl;
    command << "sudo ovs-vsctl del-br " << theSwitch->getName();

    if(this->commandExec == NULL)
        cout << "CExec NULL" << endl;

    this->commandExec->executeRemote(theSwitch->getIPOfMachine(), command.str());

}

void OVS_2_1::runSwitch(Switch* theSwitch) {


    ostringstream fileName;
    fileName << "ovs_create_"<< theSwitch->getName() << ".sh";
    ofstream fout(fileName.str().c_str());

    if(fout.is_open())
    {
        fout << "sudo ovs-vsctl add-br " << theSwitch->getName() << endl;
        if(theSwitch->getIPofController().compare("")!=0)
        {
            fout << "sudo ovs-vsctl set-controller " << theSwitch->getName()
                        << " tcp:" << theSwitch->getIPofController() 
                        << ":" << theSwitch->getPortofController() << endl;
        }
        fout << "sudo ovs-vsctl set bridge " 
             << theSwitch->getName() << " other-config:datapath-id=" 
             << theSwitch->getDPID() << endl;

        fout.close();

        
        this->commandExec->executeLocal("chmod +x "+fileName.str());
        
        this->commandExec->executeScriptRemote(theSwitch->getIPOfMachine(), "", fileName.str());

        this->commandExec->executeRemote(theSwitch->getIPOfMachine(),
               "rm "+ fileName.str());
        
        if(remove(fileName.str().c_str()) != 0 )
            LOG4CXX_ERROR((*selfLogger), "Cannot remove  OVS Script file");

    }
    else
        LOG4CXX_FATAL((*selfLogger), "Cannot create OVS Script file");


}

void OVS_2_1::attachPort(Switch* theSwitch, Interface* newInterface) {

    //hypervisor will create the tap interface
    ostringstream command;
    if(newInterface->getType() == VETH)
        command << "sudo ovs-vsctl add-port " 
                << theSwitch->getName() << " " << newInterface->getName();
    else if(newInterface->getType() == GRE)
        command << " sudo ovs-vsctl add-port " << theSwitch->getName() 
                    << " " << newInterface->getName()
                    << " -- set interface " <<  newInterface->getName()
                    << " type=gre options:remote_ip=" 
                    << ((GRE_Interface*)newInterface)->getRemoteIP() 
                    << " options:key=flow";

    //cout << command.str() << endl;
    this->commandExec->executeRemote(theSwitch->getIPOfMachine(), command.str());

}

string OVS_2_1::createClearAllRules(Switch* theSwitch) {
    ostringstream command;
    command << "sudo ovs-ofctl del-flows " << theSwitch->getName();

    return command.str();
}

void OVS_2_1::stopL2Flood(Switch* theSwitch){

    ostringstream command;
    
    command << "sudo ovs-vsctl set-fail-mode " << theSwitch->getName() << " secure";

    this->commandExec->executeRemote(theSwitch->getIPOfMachine(), command.str());
}

string OVS_2_1::createStaticTunnelRule(Switch* theSwitch,
        unsigned int tunnel_id, unsigned int input_port,
        unsigned int output_port) {

    ostringstream command;
    command << "sudo ovs-ofctl add-flow " << theSwitch->getName()
            << " in_port=" << input_port << ",actions=set_tunnel:"
            << tunnel_id << ",output:" << output_port << endl;
    command << "sudo ovs-ofctl add-flow " << theSwitch->getName()
            << " in_port=" << output_port << ",tun_id=" 
            << tunnel_id << ",actions=output:" << input_port;
    return command.str();
}
void OVS_2_1::assignPortNumber(Switch* theSwitch, Interface* newInterface) {

    ostringstream command;
    command << "sudo ovs-ofctl show " << theSwitch->getName() 
            << "| grep " << newInterface->getName() 
            << "|cut -f1 -d'('|cut -f2 -d' '";
    string portNumber = this->commandExec->executeRemote(
                                theSwitch->getIPOfMachine(), command.str());

    stringstream strStream(portNumber);
    strStream >> newInterface->port;

}
void OVS_2_1::assignStaticTunnelRule(Switch* theSwitch,
        list<pair<unsigned int, pair<unsigned int, unsigned int> > > rules) {

        ostringstream fileName;
        fileName << "ovs_static_"<< theSwitch->getName() << ".sh";
        ofstream fout(fileName.str().c_str());

        if(fout.is_open())
        {
            fout << this->createClearAllRules(theSwitch) << endl;

            for(list<pair<unsigned int, pair<unsigned int, unsigned int> > >::iterator iter = rules.begin();
                        iter != rules.end(); iter++)
            {
                fout << this->createStaticTunnelRule(theSwitch, iter->first, iter->second.first, iter->second.second) << endl;
            }


            fout.close();

            this->commandExec->executeLocal("chmod +x "+fileName.str());
            this->commandExec->executeScriptRemote(theSwitch->getIPOfMachine(), "", fileName.str());
            this->commandExec->executeRemote(theSwitch->getIPOfMachine(),
               "rm "+ fileName.str());

            if(remove(fileName.str().c_str()) != 0 )
                LOG4CXX_ERROR((*selfLogger), "Error OVS Static file");
               
        }
        else
            LOG4CXX_FATAL((*selfLogger), "Cannot create OVS Static Rule file");



}
void OVS_2_1::clearAllRules(Switch* theSwitch) {

    string command = this->createClearAllRules(theSwitch);
    this->commandExec->executeRemote(theSwitch->getIPOfMachine(), command);
}

void OVS_2_1::assignQoSToPort(Switch* theSwitch, 
    string portName, unsigned long rate, unsigned long burst)
{
    ostringstream command;
    command << "sudo ovs-vsctl set"
        << " Interface " << portName 
        << " ingress_policing_rate=" << rate;

    cout << "The policing: " << command.str() << endl;

    this->commandExec->executeRemote(theSwitch->getIPOfMachine(),
        command.str());
    
    command.str("");

    if(burst != 0)
    {
        command << "sudo ovs-vsctl set"
            << " Interface " << portName 
            << " ingress_policing_burst=" << burst;

    
        this->commandExec->executeRemote(theSwitch->getIPOfMachine(),
            command.str());
    
    }

}
void OVS_2_1::setOFVersion(Switch * theSwitch, string version)
{
    ostringstream command;

    command << "sudo ovs-vsctl set bridge " << theSwitch->getName() << " protocols=OpenFlow";
 
    //removing the .   
    command << version[0] << version[2];

    this->commandExec->executeRemote(theSwitch->getIPOfMachine(), command.str());

}

void OVS_2_1::assignIPAddress(Switch* theSwitch)
{
    ostringstream command;
    
    command << "sudo ifconfig " << theSwitch->getName() << " 10.254."
        << (128+(theSwitch->getID()+1)/256) << "." << (theSwitch->getID()+1)%256
        << "/16";
    
     this->commandExec->executeRemote(theSwitch->getIPOfMachine(), command.str());
  
}
