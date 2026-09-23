#pragma once
#include <ode/ode.h>
#include "DatamodelV2/Instance.h"
#include "DatamodelV2/PartInstance.h"
#include <vector>

class XplicitNgine : public Instance
{
public:
	XplicitNgine();
	~XplicitNgine();
	dWorldID physWorld;
	dSpaceID physSpace;
	dJointGroupID contactgroup;

	void step(float stepSize);
	void createBody(PartInstance* partInstance);
	void deleteBody(PartInstance* partInstance);
	void updateBody(PartInstance* partInstance);
	void resetBody(PartInstance* partInstance);
	void setControlState(bool forward, bool backward, bool left, bool right);

private:
	struct MotorJoint
	{
		dJointID joint;
		PartInstance* part;
		float speed;
		float side;
	};

	std::vector<dJointID> fixedJoints;
	std::vector<MotorJoint> motorJoints;
	bool controlForward;
	bool controlBackward;
	bool controlLeft;
	bool controlRight;

	void connectStuds();
	void updateMotors();
	void removeJointsForBody(dBodyID body);
};