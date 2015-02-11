/* 
 * File:   main.cpp
 * Author: gmilliez
 *
 * Created on February 5, 2015, 2:49 PM
 */

#include "SPAR/PDGFactReader.h"
#include "SPAR/PDGObjectReader.h"
#include "PDG/FactList.h"
#include "PDG/Fact.h"
#include "BELIEF_MANAGER/AddFact.h"
#include "BELIEF_MANAGER/RemoveFact.h"

// factList for each monitored agent
static std::map<unsigned int, PDG::FactList> factListMap_;

// Agents with monitored belief
static std::map<std::string, unsigned int> agentsTracked_;

// Fact is being updated
static bool factUpdating_ = false;

static unsigned int mainAgentId_ = 1;

// When adding a fact to an agent, the confidence may decrease as 
// the other's belief are suppositions based on observation

bool RemoveFactToAgent(unsigned int myFactId, unsigned int agentId) {
    factListMap_[agentId].factList.erase(factListMap_[agentId].factList.begin() + myFactId);
    return true;
}

bool RemoveFactToAgent(PDG::Fact myFact, unsigned int agentId) {
    bool removed = false;
    for (unsigned int i = 0; i < factListMap_[agentId].factList.size(); i++) {
        if ((factListMap_[agentId].factList[i].subjectId == myFact.subjectId) &&
                (factListMap_[agentId].factList[i].targetId == myFact.targetId) &&
                (factListMap_[agentId].factList[i].property == myFact.property)) {
            // as it is the same fact, we remove the previous value:
            RemoveFactToAgent(i, agentId);
            removed = true;
        }
    }
    return removed;
}

bool addFactToAgent(PDG::Fact myFact, double confidenceDecrease, unsigned int agentId) {
    //Do verify consistency
    if (myFact.propertyType == "position") {
        // We need to remove other position properties if not compatible.
        for (unsigned int i = 0; i < factListMap_[agentId].factList.size(); i++) {
            if (factListMap_[agentId].factList[i].subjectId == myFact.subjectId) {
                // If it is a position property, we remove it:
                if (factListMap_[agentId].factList[i].propertyType == "position") {
                    RemoveFactToAgent(i, agentId);
                    printf("[BELIEF_MANAGER][WARNING] Fact added to agent %d inconsistent with"
                            "current fact list: \n fact %s %s %s was removed\n",
                            agentId, factListMap_[agentId].factList[i].subjectName.c_str(),
                            factListMap_[agentId].factList[i].property.c_str(),
                            factListMap_[agentId].factList[i].targetName.c_str());
                }
            }
        }
        myFact.confidence *= confidenceDecrease;
        factListMap_[agentId].factList.push_back(myFact);
        return true;
    } else if (myFact.propertyType == "") {
        printf("[BELIEF_MANAGER][WARNING] Fact added to agent %d had no propertyType\n", agentId);
        return false;
    } else {
        // We verify that this fact is not already there.
        for (unsigned int i = 0; i < factListMap_[agentId].factList.size(); i++) {
            if ((factListMap_[agentId].factList[i].subjectId == myFact.subjectId) &&
                    (factListMap_[agentId].factList[i].targetId == myFact.targetId) &&
                    (factListMap_[agentId].factList[i].property == myFact.property)) {
                // as it is the same fact, we remove the previous value:
                RemoveFactToAgent(i, agentId);
                printf("[BELIEF_MANAGER][WARNING] Fact added to agent %d was already in"
                        "current fact list: \n fact %s %s %s was removed\n",
                        agentId, factListMap_[agentId].factList[i].subjectName.c_str(),
                        factListMap_[agentId].factList[i].property.c_str(),
                        factListMap_[agentId].factList[i].targetName.c_str());
            }
        }
        myFact.confidence *= confidenceDecrease;
        factListMap_[agentId].factList.push_back(myFact);
        return true;
    }
}

bool addFact(BELIEF_MANAGER::AddFact::Request &req,
        BELIEF_MANAGER::AddFact::Response & res) {
    // Add safely the fact to main agent
    addFactToAgent(req.fact, 1.0, mainAgentId_);
    // If fact is observable, add it to agents model if in same room.
    // alternative, add it only if agent has visibility => do this only for update
    if (req.fact.factObservability > 0.0) {
        // TODO: for each agent present and in same room
        for (std::map<std::string, unsigned int>::iterator it = agentsTracked_.begin(); it != agentsTracked_.end(); ++it) {
            // TODO: if present and in same room

            // TODO: if fact subject is not visible, decrease confidence by 50%?

            addFactToAgent(req.fact, req.fact.factObservability, it->second);
        }
    }

    res.answer = "OK";
    ROS_INFO("request: adding a new fact");
    ROS_INFO("sending back response: [%s]", res.answer.c_str());
    return true;
}

int main(int argc, char** argv) {
    // Set this in a ros service
    ros::init(argc, argv, "BELIEF_MANAGER");
    ros::NodeHandle node;

    //Initialisation boolean
    bool init = true;

    //Data reading
    PDGFactReader factRdSpark(node, "SPARK/factList");
    PDGFactReader factRdSpar(node, "SPAR/factList");
    PDGFactReader factRdAM(node, "AGENT_MONITOR/factList");
    //PDGObjectReader objectRd(node);

    //Services
    ros::ServiceServer service = node.advertiseService("add_fact", addFact);
    ROS_INFO("Ready to add fact.");

    // Agent with monitored belief
    // TODO make a ros service
    agentsTracked_["HERAKLES_HUMAN1"] = 101;

    static ros::Publisher fact_pub = node.advertise<PDG::FactList>("BELIEF_MANAGER/HERAKLES_HUMAN1/factList", 1000);

    PDG::FactList factList_msg;
    PDG::Fact fact_msg;

    // Set this in a ros service?
    ros::Rate loop_rate(30);

    // Vector of Objects.
    //std::map<std::string, Object*> mapObject;


    /************************/
    /* Start of the Ros loop*/
    /************************/

    while (node.ok()) {
        //Collect facts as 1st input and put in map
        if (init) {
            factUpdating_ = true;
            // First we feed the mainAgent belief state
            for (unsigned int i = 0; i < factRdSpar.lastMsgFact.factList.size(); i++) {
                addFactToAgent(factRdSpar.lastMsgFact.factList[i], 1.0, mainAgentId_);
            }

            for (unsigned int i = 0; i < factRdSpark.lastMsgFact.factList.size(); i++) {
                addFactToAgent(factRdSpark.lastMsgFact.factList[i], 1.0, mainAgentId_);
            }

            for (unsigned int i = 0; i < factRdAM.lastMsgFact.factList.size(); i++) {
                addFactToAgent(factRdAM.lastMsgFact.factList[i], 1.0, mainAgentId_);
            }

            // Then, if an agent is in same room, we give him the same belief
            // This is a 1st choice, it may (should?) be thought further
            for (std::map<std::string, unsigned int>::iterator it = agentsTracked_.begin(); it != agentsTracked_.end(); ++it) {

                // TODO: if present and in same room
                // TODO: if fact subject is not visible, decrease confidence by 0.3
                // if in same room and 0.1 otherwise.
                for (unsigned int i = 0; i < factListMap_[1].factList.size(); i++) {
                    addFactToAgent(factListMap_[1].factList[i], factListMap_[1].factList[i].factObservability, it->second);
                }
            }
            init = false;
            factUpdating_ = false;
        } else {


            //compare new fact to previous state.


            //update facts if needed in all models
            factUpdating_ = true;
            for (unsigned int i = 0; i < factRdSpar.lastMsgFact.factList.size(); i++) {
                addFactToAgent(factRdSpar.lastMsgFact.factList[i], 1.0, mainAgentId_);
            }

            for (unsigned int i = 0; i < factRdSpark.lastMsgFact.factList.size(); i++) {
                addFactToAgent(factRdSpark.lastMsgFact.factList[i], 1.0, mainAgentId_);
            }

            for (unsigned int i = 0; i < factRdAM.lastMsgFact.factList.size(); i++) {
                addFactToAgent(factRdAM.lastMsgFact.factList[i], 1.0, mainAgentId_);
            }
            
            // TODO: multi agent:
            // Update if:
            // 1) Agent has visibility on fact subject
            // 2) fact is observable
            
            // If fact is not observable, the supervisor will have to add manually
            // the fact to the agent. (If the robot tell something to human).            
            
            factUpdating_= false;
        }

        fact_pub.publish(factList_msg);

        ros::spinOnce();

        factList_msg.factList.clear();

        loop_rate.sleep();
    }
    return 0;
}

