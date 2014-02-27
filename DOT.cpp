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
 * DOT.cpp
 *
 *  Created on: 2013-03-18
 *      Author: Arup Raton Roy (ar3roy@uwaterloo.ca)
 */

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <float.h>
#include <map>
#include <sstream>
#include <fstream>
#include "PartitioningAlgorithm.h"
#include "Global.h"
#include "Configurations.h"
#include "LibvirtAttachment.h"
#include "GuranteedEmbedding.h"
#include "DOTTopology.h"
#include "OVS_1_10.h"
#include "DeployDOT.h"
#include "VLink.h"

using namespace std;


void prepare(string globalConfFile)
{
    Configurations *globalConf = Configurations::getConfiguration(globalConfFile);

    Global::inputTopology = InputTopology::getInputTopology();
    Global::inputTopology->populateTopology(globalConf->logicalTopologyFile);

    Global::physicalMachines = PhysicalMachines::getPhysicalMachines();
    Global::physicalMachines->populateMachines(globalConf->physicalTopologyFile);
    {
        if(globalConf->delayBetweenPhysicalMachine)
        {
            Global::physicalMachines->populateDelay(globalConf->physicalMachineDelayFile);
        }

    }

    Global::hosts = Hosts::getHosts();
    Global::hosts->populateHosts(globalConf->hostInfoFile);

    Global::controllers = Controllers::getControllers();
    Global::controllers->populateControllers(globalConf->controllerInfoFile);
    
    Global::sw2controller = Switch2Controller::getSwitch2Controller(Global::controllers);
    Global::sw2controller->loadConfiguration(globalConf->switch2ControllerFile);

    Global::credentials = Credentials::getCredentials();
    Global::credentials->populateCredentials(globalConf->credentialFile);

    Global::commandExecutor = new CommandExecutor(Global::credentials, 
        globalConf->masterIPAddress, globalConf->masterName);


    Global::mapping = Mapping::getMapping();


    PartitioningAlgorithm * partitioning = new GuranteedEmbedding(
        globalConf->partitioningAlgoConfFile, 
        Global::inputTopology, Global::physicalMachines, 
        Global::mapping, Global::hosts);

    if( partitioning->run() == false)
    {
        cout << "Embedding is not successful" << endl;
        return;
    }
        

    
    Global::instantitiatedSwitch = new OVS_1_10(Global::commandExecutor);
    Global::instantitiatedLink = new VLink(Global::commandExecutor);
    Global::instantitiatedHost = new LibvirtAttachment(globalConf, Global::hosts, 
                   Global::commandExecutor, Global::inputTopology, Global::mapping);

    

    
}

void deploy()
{
    
    DOT_Topology* dotTopology = DOT_Topology::getDOT_Topology(Global::inputTopology, 
               Global::physicalMachines, Global::mapping, 
               Global::sw2controller, Global::hosts, Global::instantitiatedHost);
    
    dotTopology->generate();

    DeployDOT *deploy = new DeployDOT(dotTopology, Global::instantitiatedSwitch, 
                            Global::instantitiatedLink, Global::instantitiatedHost, 
                            Global::commandExecutor);


}


void generateMappingForRemote()
{
  
    fstream fout("ongoing_emulation/mapping"); 
 
    fout << Global::credentials->getUserName() << endl;
 
    if(fout.is_open()) 
    { 
        for(unsigned long i = 0; i < Global::hosts->getNumberOfHosts(); i++) 
        { 
            fout << "h" << i+1 << " " 
                << Global::mapping->getMapping()->getMachine(Global::hosts->getSwitch(i)) << endl; 
        } 
        fout.close(); 
    } 
    else 
        cout << "Mapping file cannot be created" << endl; 
}


int main(int argc, char * argv[])
{

    string globalConfFile;
    if(argc == 2 && strcmp(argv[1], "-h") == 0)
    {
        cout << "-h help" << endl <<
            "-g global configuration file (default: DefaultConfiguration)" << endl;
        exit(0);
    }
    else if(argc == 3 && strcmp(argv[1],"-g") == 0)
            globalConfFile = argv[2];

    else if(argc==1)
            globalConfFile = "DefaultConfiguration";
    else
        exit(1);

    cout << globalConfFile << endl;

    prepare(globalConfFile);
    deploy();
    generateMappingForRemote();


    return 0;
}


