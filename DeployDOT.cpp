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
 * DeployDOT.cpp
 *
 *  Created on: 2013-08-28
 *      Author: Arup Raton Roy (ar3roy@uwaterloo.ca)
 */

#include "DeployDOT.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <utility>
#include "Global.h"

using namespace std;

DeployDOT::DeployDOT(DOT_Topology* dotTopology, AbstractSwitch*  abstractSwitch,
        AbstractLink* abstractLink, AbstractVM* abstractVM, CommandExecutor* commandExecutor)
{
    this->dotTopology = dotTopology;
    this->abstractVM = abstractVM;
    this->abstractLink = abstractLink;
    this->abstractSwitch = abstractSwitch;
    this->commandExecutor = commandExecutor;

    cout << "Switch Deployment Started" << endl;
    deploySwitch();
    cout << "Switch deployed" <<endl;
    deployLinks();
    cout << "link deployed" <<endl;
    deployVMs();
    cout << "VMs deployed" <<endl;
    deployTunnel();
    cout << "tunnels deployed" <<endl;
    assignPortNumbers();
    cout << "assigned port number" <<endl;
    assignStaticRules();
    cout << "static rule generated" <<endl;
    generateConsoleMapping();
    cout << "Console mapping generated" << endl;
}

DeployDOT::~DeployDOT() {

}



void DeployDOT::deploySwitch() {
    //deploy topo switch
    for( map<string, Switch*>::iterator iter = this->dotTopology->topologySwitchMap.begin();
            iter != this->dotTopology->topologySwitchMap.end(); iter++)
    {
        cout << "Clearing switch" << endl;
        this->abstractSwitch->clearSwitch(iter->second);
        cout << "Running Switch" << endl;
        this->abstractSwitch->runSwitch(iter->second);
        this->abstractSwitch->setOFVersion(iter->second, Global::otherConfig->getOFVersion());
        this->abstractSwitch->assignIPAddress(iter->second);
    }

    //deploy gateway switch
    for( map<string, Switch*>::iterator iter = this->dotTopology->gatewaySwitchMap.begin();
            iter != this->dotTopology->gatewaySwitchMap.end(); iter++)
    {
        this->abstractSwitch->clearSwitch(iter->second);
        this->abstractSwitch->runSwitch(iter->second);
        this->abstractSwitch->clearAllRules(iter->second);
        this->abstractSwitch->stopL2Flood(iter->second);
    }


}

void DeployDOT::generateConsoleMapping()
{

    ofstream fout("ongoing_emulation/mapping"); 
 
    fout << Global::credentials->getUserName() << endl;
 
    if(fout.is_open()) 
    { 
        for(unsigned long i = 0; i < this->dotTopology->vms->getNumberOfVMs(); i++) 
        { 
            unsigned long switchId = this->dotTopology->vms->getSwitch(i);
    
            ostringstream switchName;

            switchName << "topo" << switchId+1;

            //cout << "Host is attached to " << switchName.str() << endl;

            Switch* switchOfHost = this->dotTopology->topologySwitchMap[switchName.str()];

            fout << "h" << i+1 << " " 
                << switchOfHost->getIPOfMachine() << " "
                << this->dotTopology->vms->getImageId(i)+1    << endl; 
        } 
        fout.close(); 
    } 
    else 
        cout << "Mapping file for \"DOT Console\" cannot be created" << endl; 


    fout.clear();
    fout.open("ongoing_emulation/switch_mapping"); 
 
 
    if(fout.is_open()) 
    { 
        for(unsigned long i = 0; i < Global::logicalTopology->getNumberOfSwitches(); i++) 
        { 
            fout << "s" << i+1 << " " 
                << Global::mapping->getMapping()->getMachine(i) << endl; 
        } 
        fout.close(); 
    } 
    else 
          cout << "Switch Mapping file for \"DOT Console\" cannot be created" <<endl; 

    fout.clear();
    fout.open("ongoing_emulation/link_mapping"); 
 
     if(fout.is_open())
     {
         //generate the itra-host links
         for(map<unsigned long, Link*>::iterator iter = this->dotTopology->linkMap.begin();
                iter != this->dotTopology->linkMap.end(); iter ++)
         {

             Switch* switch1 = iter->second->getInterface1()->getSwitch();
             Switch* switch2 = iter->second->getInterface2()->getSwitch();

             if(switch1->getName().substr(0,2).compare("gw") != 0 
                && switch2->getName().substr(0,2).compare("gw") != 0)

                fout << "s" << switch1->getID()+1 << " " 
                    << iter->second->getInterface1()->getName() << " "
                    << "s" << switch2->getID()+1 << " " 
                    << iter->second->getInterface2()->getName() << endl; 
        }

        //generating the iter-host links
        for(vector<pair<CutEdge*, CutEdge*> >::iterator iter = this->dotTopology->cutEdgesPairVector.begin();
            iter != this->dotTopology->cutEdgesPairVector.end(); iter++)
        {
            CutEdge* cutEdge1 = iter->first;
            CutEdge* cutEdge2 = iter->second;

            Interface* interface1 = cutEdge1->getInterface1();
            Interface* interface2 = cutEdge2->getInterface1();

            Switch* switch1 = interface1->getSwitch();
            Switch* switch2 = interface2->getSwitch();

            fout << "s" << switch1->getID()+1 << " "
                 << interface1->getName() << " "
                 << "s" << switch2->getID()+1 << " "
                 << interface2->getName() << endl;

        }

        fout.close();
    }

    else 
          cout << "Link Mapping file for \"DOT Console\" cannot be created" <<endl; 

    fout.clear();

    fout.open("ongoing_emulation/ctrl_mapping");
    if(fout.is_open())
    {
        //OF version configured
        fout << Global::otherConfig->getOFVersion() << endl;
        
        map<unsigned int, pair<string, string> > activeControllerMap;
      
        for(unsigned long i = 0; i < Global::logicalTopology->getNumberOfSwitches(); i++)
        {
           
            activeControllerMap[Global::sw2controller->getControllerId(i)] 
                = make_pair(Global::sw2controller->getControllerIP(i),
                        Global::sw2controller->getControllerPort(i));
        }

        for(map<unsigned int, pair<string, string> >::iterator iter = activeControllerMap.begin();
            iter != activeControllerMap.end(); iter++)
            fout << "c" << iter->first+1 << " " << iter->second.first 
                << " " << iter->second.second << endl;

    
        fout.close();


    }
    else
         cout << "Contrlller mapping file cannot be generated" << endl;

}
void DeployDOT::deployVMs() {

    this->abstractVM->prepare();
    cout << "Virsh Network Created" <<endl;
    for(unsigned long i = 0; i < this->dotTopology->vms->getNumberOfVMs(); i++)
    {
        this->abstractVM->startHost(i);
        this->abstractVM->retrieveInterface(i);

        unsigned long switchId = this->dotTopology->vms->getSwitch(i);
        
        ostringstream switchName;
        
        switchName << "topo" << switchId+1;
       
        cout << "Host is attached to " << switchName.str() << endl;
 
        Switch* switchOfHost = this->dotTopology->topologySwitchMap[switchName.str()];

        this->abstractSwitch->assignQoSToPort(switchOfHost,
            this->dotTopology->vms->getInterfaceName(i),
            this->dotTopology->vms->getBandwidth(i)*1000);
    }
}

void DeployDOT::deployLinks() {

    for(map<unsigned long, Link*>::iterator iter = this->dotTopology->linkMap.begin();
            iter != this->dotTopology->linkMap.end(); iter ++)
    {
        this->abstractLink->createLink(iter->second);

        //assigning to ovs
        this->abstractSwitch->attachPort(iter->second->getInterface1()->getSwitch(), iter->second->getInterface1());
        this->abstractSwitch->attachPort(iter->second->getInterface2()->getSwitch(), iter->second->getInterface2());


        //assigning rate limit to interface
        this->abstractSwitch->assignQoSToPort(iter->second->getInterface1()->getSwitch(),
            iter->second->getInterface1()->getName(), 
            iter->second->getBandwidth()*1000);
        this->abstractSwitch->assignQoSToPort(iter->second->getInterface2()->getSwitch(),
            iter->second->getInterface2()->getName(),
            iter->second->getBandwidth()*1000);

    }
}

void DeployDOT::deployTunnel() {

    for(unsigned int i = 0; i < this->dotTopology->physicalMachines->getNumberOfPhysicalMachines() - 1; i++)
        for(unsigned int j = i+1; j < this->dotTopology->physicalMachines->getNumberOfPhysicalMachines(); j++)
        {
            string IPAddressMachine1 = this->dotTopology->physicalMachines->getIPAddress(i);
            string IPAddressMachine2 = this->dotTopology->physicalMachines->getIPAddress(j);

            Interface* interface1 = this->dotTopology->tunnelMap[make_pair(IPAddressMachine1, IPAddressMachine2)]->getInterface1();
            this->abstractSwitch->attachPort(interface1->getSwitch(),interface1);

            Interface* interface2 = this->dotTopology->tunnelMap[make_pair(IPAddressMachine1, IPAddressMachine2)]->getInterface2();
            this->abstractSwitch->attachPort(interface2->getSwitch(),interface2);

        }
}

void DeployDOT::assignPortNumbers() {

    for(map<pair<Switch*, string>, Interface* >::iterator iter = this->dotTopology->interfaceMap.begin();
            iter != this->dotTopology->interfaceMap.end(); iter++)
    {
        this->abstractSwitch->assignPortNumber(iter->second->getSwitch(), iter->second);
    }
}

void DeployDOT::assignStaticRules() {

    map<Switch*, list<pair<unsigned int, pair<unsigned int, unsigned int> > > > ruleMap;
    for(vector<pair<CutEdge*, CutEdge*> >::iterator iter = this->dotTopology->cutEdgesPairVector.begin();
             iter != this->dotTopology->cutEdgesPairVector.end(); iter++)
    {
        string IPAddressMachine1 = iter->first->getInterface1()->getSwitch()->getIPOfMachine();
        string IPAddressMachine2 = iter->second->getInterface1()->getSwitch()->getIPOfMachine();
        cout << "IP of M1" << IPAddressMachine1;
        cout << "IP of M2" << IPAddressMachine2 << endl;

        Interface* tunnelInterfaceMachine1 = iter->first->getAttachedTunnel()->getInterface1();
        Interface* tunnelInterfaceMachine2 = iter->first->getAttachedTunnel()->getInterface2();

        cout << "IP of tunnel interface1" << tunnelInterfaceMachine1->getSwitch()->getIPOfMachine();
        cout << "interface2 " << tunnelInterfaceMachine2->getSwitch()->getIPOfMachine() << endl;

        Interface* interMachine1;
        Interface* interMachine2;

        if(IPAddressMachine1.compare(tunnelInterfaceMachine1->getSwitch()->getIPOfMachine())==0)
        {
            interMachine1 = tunnelInterfaceMachine1;
            interMachine2 = tunnelInterfaceMachine2;
        }
        else if(IPAddressMachine2.compare(tunnelInterfaceMachine1->getSwitch()->getIPOfMachine())==0)
        {
            interMachine2 = tunnelInterfaceMachine1;
            interMachine1 = tunnelInterfaceMachine2;
        }
        else
            cout << "Tunnel Rule Generation Error" << endl;

        cout << this->abstractSwitch->createStaticTunnelRule(iter->first->getInterface2()->getSwitch(), iter->first->cut_edge_id,
                iter->first->getInterface2()->port, interMachine1->port) << endl;

        cout << this->abstractSwitch->createStaticTunnelRule(iter->second->getInterface2()->getSwitch(), iter->second->cut_edge_id,
                        iter->second->getInterface2()->port, interMachine2->port) << endl;

        ruleMap[iter->first->getInterface2()->getSwitch()].push_back(make_pair(iter->first->cut_edge_id,
                make_pair(iter->first->getInterface2()->port, interMachine1->port)));
        ruleMap[iter->second->getInterface2()->getSwitch()].push_back(make_pair(iter->second->cut_edge_id,
                        make_pair(iter->second->getInterface2()->port, interMachine2->port)));
    }

    for(map<Switch*, list<pair<unsigned int, pair<unsigned int, unsigned int> > > >::iterator iter = ruleMap.begin();
            iter != ruleMap.end(); iter++)
    {
        this->abstractSwitch->assignStaticTunnelRule(iter->first, iter->second);
    }

}

