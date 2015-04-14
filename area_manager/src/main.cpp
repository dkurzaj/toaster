//This file will compute the spatial facts concerning agents present in the interaction.

#include "area_manager/PDGHumanReader.h"
#include "area_manager/PDGRobotReader.h"
#include "area_manager/PDGObjectReader.h"
#include "toaster-lib/CircleArea.h"
#include "toaster-lib/PolygonArea.h"
#include "toaster-lib/MathFunctions.h"
#include "toaster-lib/Object.h"
#include <pdg/Fact.h>
#include <pdg/FactList.h>
#include <iterator>

// Entity should be a vector or a map with all entities
// This function update all the area that depends on an entity position.

void updateEntityArea(std::map<unsigned int, Area*>& mpArea, Entity* entity) {
    for (std::map<unsigned int, Area*>::iterator it = mpArea.begin(); it != mpArea.end(); ++it) {
        if (it->second->getMyOwner() == entity->getId())
            // Dangerous casting? Only CircleArea has Owner...
            ((CircleArea*) it->second)->setCenter(MathFunctions::convert3dTo2d(entity->getPosition()));
    }
}

void updateInArea(Entity* ent, std::map<unsigned int, Area*>& mpArea) {
    for (std::map<unsigned int, Area*>::iterator it = mpArea.begin(); it != mpArea.end(); ++it) {
        // If we already know that entity is in Area, we update if needed.
        if (ent->isInArea(it->second->getId()))
            if (it->second->isPointInArea(MathFunctions::convert3dTo2d(ent->getPosition())))
                continue;
            else {
                printf("[area_manager] %s leaves Area %s\n", ent->getName().c_str(), it->second->getName().c_str());
                ent->removeInArea(it->second->getId());
                it->second->removeEntity(ent->getId());
            if (it->second->getIsRoom()) 
                ent->setRoomId(0);
            }// Same if entity is not in Area
        else
            if (it->second->isPointInArea(MathFunctions::convert3dTo2d(ent->getPosition()))) {
            printf("[area_manager] %s enters in Area %s\n", ent->getName().c_str(), it->second->getName().c_str());
            ent->inArea_.push_back(it->second->getId());
            it->second->insideEntities_.push_back(ent->getId());

            //User has to be in a room. May it be a "global room".
            if (it->second->getIsRoom())
                ent->setRoomId(it->second->getId());
        } else {
            //printf("[area_manager][DEGUG] %s is not in Area %s, he is in %f, %f\n", ent->getName().c_str(), it->second->getName().c_str(), ent->getPosition().get<0>(), ent->getPosition().get<1>());
            continue;
        }
    }
}

// Return confidence: 0.0 if not facing 1.0 if facing

double isFacing(Entity* entFacing, Entity* entSubject, double angleThreshold) {
    return MathFunctions::isInAngle(entFacing, entSubject, entFacing->getOrientation()[2], angleThreshold);
}

int main(int argc, char** argv) {
    // Set this in a ros service
    const bool AGENT_FULL_CONFIG = false; //If false we will use only position and orientation

    ros::init(argc, argv, "area_manager");
    ros::NodeHandle node;

    //Data reading
    PDGHumanReader humanRd(node, AGENT_FULL_CONFIG);
    PDGRobotReader robotRd(node, AGENT_FULL_CONFIG);
    PDGObjectReader objectRd(node);


    ros::Publisher fact_pub = node.advertise<pdg::FactList>("area_manager/factList", 1000);

    pdg::FactList factList_msg;
    pdg::Fact fact_msg;

    // Set this in a ros service?
    ros::Rate loop_rate(30);

    // Vector of Area
    // It should be possible to add an area on the fly with a ros service.
    std::map<unsigned int, Area*> mapArea;

    // Origin point
    bg::model::point<double, 2, bg::cs::cartesian> origin(0.0, 0.0);

    // AreaRays:
    double interDist = 5.0;
    double dangerDist = 1.5;

    // TODO: put this in a service with parameters
    // We create 2 Area for the robot:
    // Interacting
    CircleArea* interacting = new CircleArea(0, origin, interDist);
    interacting->setMyOwner(1);
    interacting->setName("pr2_interacting");
    interacting->setIsRoom(false);
    // Danger
    CircleArea* danger = new CircleArea(1, origin, dangerDist);
    danger->setMyOwner(1);
    danger->setName("pr2_danger");
    danger->setIsRoom(false);

    // We define here some room (Adream)
    double pointsBed[5][2] = {
        {1.9, 13.1},
        {6.0, 13.1},
        {6.0, 9.0},
        {1.9, 9.0},
        {1.9, 13.1}
    };
    double pointsKitch[5][2] = {
        {6.0, 13.1},
        {9.0, 13.1},
        {9.0, 9.0},
        {6.0, 9.0},
        {6.0, 13.1}
    };
    double pointsLiving[5][2] = {
        {1.9, 9.0},
        {9.0, 9.0},
        {9.0, 5.0},
        {1.9, 5.0},
        {1.9, 9.0}
    };
    PolygonArea* bedroom = new PolygonArea(2, pointsBed, 5);
    bedroom->setName("bedroom");
    bedroom->setIsRoom(true);

    PolygonArea* kitchen = new PolygonArea(3, pointsKitch, 5);
    kitchen->setName("kitchen");
    kitchen->setIsRoom(true);

    PolygonArea* livingroom = new PolygonArea(4, pointsLiving, 5);
    livingroom->setName("livingroom");
    livingroom->setIsRoom(true);

    mapArea[0] = interacting;
    mapArea[1] = danger;
    mapArea[2] = bedroom;
    mapArea[3] = kitchen;
    mapArea[4] = livingroom;

    /************************/
    /* Start of the Ros loop*/
    /************************/

    while (node.ok()) {
        if ((humanRd.lastConfig_[101] != NULL)  && (robotRd.lastConfig_[1] != NULL)) {
	    // We update area with robot center
            //TODO: Update this only if they are in same room?
            if (robotRd.lastConfig_[1] != NULL)
                updateEntityArea(mapArea, robotRd.lastConfig_[1]);

            // We update entities vector inArea
            // TODO: actually do this for every entities!
            updateInArea(humanRd.lastConfig_[101], mapArea);
            updateInArea(robotRd.lastConfig_[1], mapArea);
            for(std::map<unsigned int, Object*>::const_iterator it=objectRd.lastConfig_.begin() ; it!=objectRd.lastConfig_.end() ; ++it)
            {
              updateInArea(objectRd.lastConfig_[it->first], mapArea);
            }

            if (humanRd.lastConfig_[101]->isInArea(0)) {

                //Fact Area
                fact_msg.property = "IsInArea";
                fact_msg.propertyType = "position";
                fact_msg.subProperty = "interacting";
                fact_msg.subjectId = 101;
                fact_msg.subjectName =  humanRd.lastConfig_[101]->getName();
                fact_msg.targetName = robotRd.lastConfig_[1]->getName();
                fact_msg.targetId = 1;
                fact_msg.confidence = 1.0;
                fact_msg.factObservability = 1.0;
                fact_msg.time = humanRd.lastConfig_[101]->getTime();

                factList_msg.factList.push_back(fact_msg);

                // We will compute here facts that are relevant for interacting
                if (robotRd.lastConfig_[1] != NULL) {
                    double confidence = 0.0;
                    confidence = isFacing(humanRd.lastConfig_[101], robotRd.lastConfig_[1], 0.5);
                    if (confidence > 0.0) {
                        printf("[area_manager][DEGUG] %s is facing %s with confidence %f\n",
                                humanRd.lastConfig_[101]->getName().c_str(), robotRd.lastConfig_[1]->getName().c_str(), confidence);

                        //Fact
                        fact_msg.property = "IsFacing";
                        fact_msg.propertyType = "posture";
                        fact_msg.subProperty = "orientationAngle";
                        fact_msg.subjectId = 101;
                        fact_msg.subjectName =  humanRd.lastConfig_[101]->getName();
                        fact_msg.targetName = robotRd.lastConfig_[1]->getName();
                        fact_msg.targetId = 1;
                        fact_msg.confidence = confidence;
                        fact_msg.factObservability = 1.0;
                        fact_msg.time = humanRd.lastConfig_[101]->getTime();

                        factList_msg.factList.push_back(fact_msg);
                    }
                }
            } else if (humanRd.lastConfig_[101]->isInArea(1)) {
                // We will compute here facts that are relevant when human is in danger zone

                //Fact
                fact_msg.property = "IsInArea";
                fact_msg.propertyType = "position";
                fact_msg.subProperty = "Danger";
                fact_msg.subjectId = 101;
                fact_msg.targetId = 1;
                fact_msg.subjectName =  humanRd.lastConfig_[101]->getName();
                fact_msg.targetName = robotRd.lastConfig_[1]->getName();
                fact_msg.confidence = 1;
                fact_msg.factObservability = 1.0;
                fact_msg.time = humanRd.lastConfig_[101]->getTime();

                factList_msg.factList.push_back(fact_msg);
            } else {
                // We will compute here facts that are relevant for human out of interacting zone
            }

            std::string roomName;

            // TODO: For each entities
            for(std::map<unsigned int, Object*>::const_iterator it=objectRd.lastConfig_.begin() ; it!=objectRd.lastConfig_.end() ; ++it)
            {
              printf("[area_manager][DEBUG] object %s is in room %d\n", objectRd.lastConfig_[it->first]->getName().c_str(), objectRd.lastConfig_[it->first]->getRoomId());

              if(objectRd.lastConfig_[it->first]->getRoomId() == 0)
                roomName = "global";
              else
                roomName = mapArea[objectRd.lastConfig_[it->first]->getRoomId()]->getName();

              //Fact room
              fact_msg.property = "IsInArea";
              fact_msg.propertyType = "position";
              fact_msg.subProperty = "room";
              fact_msg.subjectId = it->first;
              fact_msg.subjectName =  objectRd.lastConfig_[it->first]->getName();
              fact_msg.targetId = objectRd.lastConfig_[it->first]->getRoomId();
              fact_msg.targetName = roomName;
              fact_msg.confidence = 0.99;
              fact_msg.factObservability = 1.0;
              fact_msg.time = objectRd.lastConfig_[it->first]->getTime();
 
            }

            if(humanRd.lastConfig_[101]->getRoomId() == 0)
                roomName = "global";
              else
                roomName = mapArea[humanRd.lastConfig_[101]->getRoomId()]->getName();            
 
            //Fact room
            fact_msg.property = "IsInArea";
            fact_msg.propertyType = "position";
            fact_msg.subProperty = "room";
            fact_msg.subjectId = 101;
            fact_msg.subjectName =  humanRd.lastConfig_[101]->getName();
            fact_msg.targetId = humanRd.lastConfig_[101]->getRoomId();
            fact_msg.targetName = roomName;
            fact_msg.confidence = 0.99;
            fact_msg.factObservability = 1.0;
            fact_msg.time = humanRd.lastConfig_[101]->getTime();

            factList_msg.factList.push_back(fact_msg);

        }
        fact_pub.publish(factList_msg);

        ros::spinOnce();

        factList_msg.factList.clear();

        loop_rate.sleep();
    }
    return 0;
}