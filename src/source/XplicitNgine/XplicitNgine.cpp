#include "XplicitNgine/XplicitNgine.h"
#include "Globals.h"
#include "DataModelV2/GroupInstance.h"
#include "Faces.h"
#include <algorithm>
#include <math.h>
#include <windows.h>

struct FaceInfo
{
	Vector3 center;
	Vector3 normal;
	Vector3 axisU;
	Vector3 axisV;
	float halfU;
	float halfV;
};

static void getFaceInfo(PartInstance* part, int face, FaceInfo& info)
{
	CoordinateFrame frame = part->getCFrame();
	Vector3 size = part->getSize();
	Vector3 position = part->getPosition();
	Vector3 right = frame.rightVector();
	Vector3 up = frame.upVector();
	Vector3 look = frame.lookVector();

	if (face == TOP || face == BOTTOM)
	{
		info.normal = face == TOP ? up : -up;
		info.center = position + info.normal * (size.y / 2.0f);
		info.axisU = right;
		info.axisV = look;
		info.halfU = size.x / 2.0f;
		info.halfV = size.z / 2.0f;
	}
	else if (face == LEFT || face == RIGHT)
	{
		info.normal = face == RIGHT ? right : -right;
		info.center = position + info.normal * (size.x / 2.0f);
		info.axisU = look;
		info.axisV = up;
		info.halfU = size.z / 2.0f;
		info.halfV = size.y / 2.0f;
	}
	else
	{
		info.normal = face == FRONT ? look : -look;
		info.center = position + info.normal * (size.z / 2.0f);
		info.axisU = right;
		info.axisV = up;
		info.halfU = size.x / 2.0f;
		info.halfV = size.y / 2.0f;
	}
}

static bool facesTouch(
	PartInstance* first, int firstFace,
	PartInstance* second, int secondFace,
	float planeTolerance,
	float overlapPadding)
{
	FaceInfo a;
	FaceInfo b;
	getFaceInfo(first, firstFace, a);
	getFaceInfo(second, secondFace, b);

	if (a.normal.dot(b.normal) > -0.5f)
		return false;

	Vector3 delta = b.center - a.center;
	if (fabs(delta.dot(a.normal)) > planeTolerance)
		return false;

	float uOverlap =
		a.halfU + fabs(a.axisU.dot(b.axisU)) * b.halfU +
		fabs(a.axisU.dot(b.axisV)) * b.halfV;
	float vOverlap =
		a.halfV + fabs(a.axisV.dot(b.axisU)) * b.halfU +
		fabs(a.axisV.dot(b.axisV)) * b.halfV;

	return fabs(delta.dot(a.axisU)) <= uOverlap + overlapPadding &&
		   fabs(delta.dot(a.axisV)) <= vOverlap + overlapPadding;
}

static Enum::SurfaceType::Value oppositeSurface(
	PartInstance* part, int face)
{
	return part->getSurface(face);
}

static bool isMotor(Enum::SurfaceType::Value surface)
{
	return surface == Enum::SurfaceType::Motor ||
		   surface == Enum::SurfaceType::StepperMotor;
}

static GroupInstance* getOwningGroup(PartInstance* part)
{
	Instance* current = part->getParent();
	while (current != NULL)
	{
		GroupInstance* group = dynamic_cast<GroupInstance*>(current);
		if (group != NULL)
			return group;
		current = current->getParent();
	}
	return NULL;
}

static Enum::Controller::Value getMotorController(PartInstance* part)
{
	if (part->controller != Enum::Controller::None)
		return part->controller;

	GroupInstance* group = getOwningGroup(part);
	if (group != NULL)
		return group->controller;

	return Enum::Controller::None;
}

XplicitNgine::XplicitNgine() 
{
	
	physWorld = dWorldCreate();
	physSpace = dHashSpaceCreate(0);
	contactgroup = dJointGroupCreate(0);

	dWorldSetGravity(physWorld, 0, -9.8F, 0);
	dWorldSetAutoDisableFlag(physWorld, 1);
	dWorldSetAutoDisableLinearThreshold(physWorld, 0.5F);
	dWorldSetAutoDisableAngularThreshold(physWorld, 0.5F);
	dWorldSetAutoDisableSteps(physWorld, 20);

	this->name = "PhysicsService";
	controlForward = false;
	controlBackward = false;
	controlLeft = false;
	controlRight = false;
}

XplicitNgine::~XplicitNgine() 
{
  dJointGroupDestroy (contactgroup);
  dSpaceDestroy (physSpace);
  dWorldDestroy (physWorld);
  dCloseODE();
}

void XplicitNgine::resetBody(PartInstance* partInstance)
{
	deleteBody(partInstance);
	createBody(partInstance);
}

void XplicitNgine::setControlState(
	bool forward, bool backward, bool left, bool right)
{
	controlForward = forward;
	controlBackward = backward;
	controlLeft = left;
	controlRight = right;
}

void XplicitNgine::removeJointsForBody(dBodyID body)
{
	for (size_t i = 0; i < fixedJoints.size();)
	{
		dBodyID first = dJointGetBody(fixedJoints[i], 0);
		dBodyID second = dJointGetBody(fixedJoints[i], 1);
		if (first == body || second == body)
		{
			dJointDestroy(fixedJoints[i]);
			fixedJoints.erase(fixedJoints.begin() + i);
		}
		else
			++i;
	}

	for (size_t i = 0; i < motorJoints.size();)
	{
		dBodyID first = dJointGetBody(motorJoints[i].joint, 0);
		dBodyID second = dJointGetBody(motorJoints[i].joint, 1);
		if (first == body || second == body)
		{
			dJointDestroy(motorJoints[i].joint);
			motorJoints.erase(motorJoints.begin() + i);
		}
		else
			++i;
	}
}

void XplicitNgine::connectStuds()
{
	std::vector<Instance*> all =
		g_dataModel->getWorkspace()->getAllChildren();
	std::vector<PartInstance*> parts;

	for (size_t i = 0; i < all.size(); ++i)
	{
		PartInstance* part = dynamic_cast<PartInstance*>(all[i]);
		if (part != NULL && part->physBody != NULL)
			parts.push_back(part);
	}

	for (size_t i = 0; i < parts.size(); ++i)
	{
		for (size_t j = i + 1; j < parts.size(); ++j)
		{
			PartInstance* first = parts[i];
			PartInstance* second = parts[j];

			if (first->physBody == NULL || second->physBody == NULL)
				continue;

			for (int face = 0; face < 6; ++face)
			{
				int oppositeFace =
					face == TOP ? BOTTOM :
					face == BOTTOM ? TOP :
					face == LEFT ? RIGHT :
					face == RIGHT ? LEFT :
					face == FRONT ? BACK : FRONT;

				Enum::SurfaceType::Value firstSurface =
					oppositeSurface(first, face);
				Enum::SurfaceType::Value secondSurface =
					oppositeSurface(second, oppositeFace);

				bool motorFace = isMotor(firstSurface) ||
					isMotor(secondSurface);
				if (!facesTouch(
					first,
					face,
					second,
					oppositeFace,
					motorFace ? 1.20f : 0.60f,
					motorFace ? 0.75f : 0.0f))
					continue;

				if (motorFace)
				{
					bool motorAlreadyExists = false;
					for (size_t jointIndex = 0;
						jointIndex < motorJoints.size(); ++jointIndex)
					{
						dBodyID jointFirst = dJointGetBody(
							motorJoints[jointIndex].joint, 0);
						dBodyID jointSecond = dJointGetBody(
							motorJoints[jointIndex].joint, 1);
						if ((jointFirst == first->physBody &&
							 jointSecond == second->physBody) ||
							(jointFirst == second->physBody &&
							 jointSecond == first->physBody))
						{
							motorAlreadyExists = true;
							break;
						}
					}
					if (motorAlreadyExists)
						continue;

					PartInstance* motorPart =
						isMotor(firstSurface) ? first : second;
					int motorFaceIndex =
						isMotor(firstSurface) ? face : oppositeFace;
					int motorParam =
						motorPart->getSurfaceParam(motorFaceIndex);

					FaceInfo motorInfo;
					getFaceInfo(motorPart, motorFaceIndex, motorInfo);
					FaceInfo otherInfo;
					getFaceInfo(
						motorPart == first ? second : first,
						motorPart == first ? oppositeFace : face,
						otherInfo);
					Vector3 hingeAnchor =
						(motorInfo.center + otherInfo.center) * 0.5f;

					dJointID hinge = dJointCreateHinge(physWorld, 0);
					dBodyID firstBody =
						first->isAnchored() ? 0 : first->physBody;
					dBodyID secondBody =
						second->isAnchored() ? 0 : second->physBody;
					dJointAttach(
						hinge,
						firstBody,
						secondBody
					);
					dJointSetHingeAnchor(
						hinge,
						hingeAnchor.x,
						hingeAnchor.y,
						hingeAnchor.z
					);
					dJointSetHingeAxis(
						hinge,
						motorInfo.normal.x,
						motorInfo.normal.y,
						motorInfo.normal.z
					);

					MotorJoint state;
					state.joint = hinge;
					state.part = motorPart;
					state.speed = (motorParam == 2 || motorParam == 4) ? 8.0f : 4.0f;
					state.side = (motorParam == 3 || motorParam == 4) ? 1.0f : -1.0f;
					motorJoints.push_back(state);
					break;
				}

				if (firstSurface != Enum::SurfaceType::Bumps &&
					secondSurface != Enum::SurfaceType::Bumps)
					continue;

				// Cylinders are wheels/rotors and must not be frozen by studs.
				if (first->shape == Enum::Shape::Cylinder ||
					second->shape == Enum::Shape::Cylinder)
					continue;

				bool fixedAlreadyExists = false;
				for (size_t jointIndex = 0;
					jointIndex < fixedJoints.size(); ++jointIndex)
				{
					dBodyID jointFirst = dJointGetBody(
						fixedJoints[jointIndex], 0);
					dBodyID jointSecond = dJointGetBody(
						fixedJoints[jointIndex], 1);
					if ((jointFirst == first->physBody &&
						 jointSecond == second->physBody) ||
						(jointFirst == second->physBody &&
						 jointSecond == first->physBody))
					{
						fixedAlreadyExists = true;
						break;
					}
				}
				if (fixedAlreadyExists)
					continue;

				dJointID fixed = dJointCreateFixed(physWorld, 0);
				dJointAttach(
					fixed,
					first->isAnchored() ? 0 : first->physBody,
					second->isAnchored() ? 0 : second->physBody
				);
				dJointSetFixed(fixed);
				fixedJoints.push_back(fixed);
				break;
			}
		}
	}
}

void XplicitNgine::updateMotors()
{
	bool keyForward =
		(GetAsyncKeyState(VK_UP) & 0x8000) != 0 ||
		(GetAsyncKeyState('W') & 0x8000) != 0;
	bool keyBackward =
		(GetAsyncKeyState(VK_DOWN) & 0x8000) != 0 ||
		(GetAsyncKeyState('S') & 0x8000) != 0;
	bool keyLeft =
		(GetAsyncKeyState(VK_LEFT) & 0x8000) != 0 ||
		(GetAsyncKeyState('A') & 0x8000) != 0;
	bool keyRight =
		(GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0 ||
		(GetAsyncKeyState('D') & 0x8000) != 0;

	float forward =
		((controlForward || keyForward) ? 1.0f : 0.0f) -
		((controlBackward || keyBackward) ? 1.0f : 0.0f);
	float turn =
		((controlRight || keyRight) ? 1.0f : 0.0f) -
		((controlLeft || keyLeft) ? 1.0f : 0.0f);

	for (size_t i = 0; i < motorJoints.size(); ++i)
	{
		Enum::Controller::Value controller =
			getMotorController(motorJoints[i].part);
		float velocity = motorJoints[i].side * motorJoints[i].speed;

		if (controller != Enum::Controller::None)
		{
			velocity =
				(forward + turn * motorJoints[i].side) *
				motorJoints[i].speed;
		}

		dJointSetHingeParam(
			motorJoints[i].joint, dParamVel, velocity);
		dJointSetHingeParam(
			motorJoints[i].joint, dParamFMax, 1000.0f);
	}
}

void collisionCallback(void *data, dGeomID o1, dGeomID o2) 
{
	int i,n;
	
	dBodyID b1 = dGeomGetBody(o1);
	dBodyID b2 = dGeomGetBody(o2);
	
	if (b1 && b2 && dAreConnected(b1, b2))
		return;
	
	

	const int N = 4;
	dContact contact[N];
	n = dCollide (o1,o2,N,&contact[0].geom,sizeof(dContact));
	if (n > 0) {
		for (i=0; i<n; i++) {
			contact[i].surface.mode = dContactBounce | dContactSlip1 | dContactSlip2 | dContactSoftERP | dContactSoftCFM | dContactApprox1;

			// Define contact surface properties
			contact[i].surface.bounce = 0.5F; //Elasticity
			contact[i].surface.mu = 0.4F; //Friction
			contact[i].surface.slip1 = 0.0;
			contact[i].surface.slip2 = 0.0;
			contact[i].surface.soft_erp = 0.8F;
			contact[i].surface.soft_cfm = 0.005F;
			
			// Create joints
			dJointID c = dJointCreateContact(
				g_xplicitNgine->physWorld,
				g_xplicitNgine->contactgroup,
				contact+i
			);
			
			dJointAttach (c,b1,b2);
			
			if(b1 != NULL) 
			{
				PartInstance* touched = (PartInstance*)dGeomGetData(o2);
				if(touched != NULL) 
				{
					touched->onTouch();
				}
			}
		}
	}
}

void XplicitNgine::deleteBody(PartInstance* partInstance)
{
	if(partInstance->physBody != NULL)
	{
		removeJointsForBody(partInstance->physBody);
		dBodyEnable(partInstance->physBody);
		dGeomEnable(partInstance->physGeom[0]);
		if(partInstance->isAnchored() || partInstance->isDragging())
		{
			dGeomSetBody(partInstance->physGeom[0], partInstance->physBody);
			dGeomEnable(partInstance->physGeom[0]);
			updateBody(partInstance);
			step(0.03F);
		}

		while (dBodyGetNumJoints(partInstance->physBody) > 0) {
			dJointID joint =
				dBodyGetJoint(partInstance->physBody, 0);
			dBodyID b1 = dJointGetBody(joint, 0);
			dBodyID b2 = dJointGetBody(joint, 1);
			
			if(b1 != NULL)
			{
				dBodyEnable(b1);
				PartInstance * part = (PartInstance *)dBodyGetData(b1);
				if(part != NULL)
					dGeomEnable(part->physGeom[0]);
			}

			if(b2 != NULL)
			{
				dBodyEnable(b2);
				PartInstance * part = (PartInstance *)dBodyGetData(b2);
				if(part != NULL)
					dGeomEnable(part->physGeom[0]);
			}
			dJointDestroy(joint);
		}
		dBodyDestroy(partInstance->physBody);
		dGeomDestroy(partInstance->physGeom[0]);
		partInstance->physBody = NULL;
	}
}

void XplicitNgine::createBody(PartInstance* partInstance)
{
	if (partInstance == NULL || partInstance->isDragging())
		return;
	if(partInstance->physBody == NULL) 
	{
		
		Vector3 partSize = partInstance->getSize();
		Vector3 partPosition = partInstance->getPosition();
		Vector3 velocity = partInstance->getVelocity();
		Vector3 rotVelocity = partInstance->getRotVelocity();
		
		// init body
		partInstance->physBody = dBodyCreate(physWorld);
		dBodySetData(partInstance->physBody, partInstance);
		
		
		// Create geom
		if(partInstance->shape == Enum::Shape::Block)
		{
			partInstance->physGeom[0] = dCreateBox(physSpace,
					partSize.x,
					partSize.y,
					partSize.z
				);

			dVector3 result;
			dGeomBoxGetLengths(partInstance->physGeom[0], result);
		}
		else if (partInstance->shape == Enum::Shape::Cylinder)
		{
			float radius = partSize.y;
			if (partSize.z < radius)
				radius = partSize.z;
			radius *= 0.5f;
			partInstance->physGeom[0] =
				dCreateCylinder(physSpace, radius, partSize.x);
		}
		else
		{
			partInstance->physGeom[0] = dCreateSphere(physSpace, partSize[0]/2);
		}
		
		if(partInstance->physGeom[0])
			dGeomSetData(partInstance->physGeom[0], partInstance);

		dMass mass;
		if (partInstance->shape == Enum::Shape::Block)
		{
			dMassSetBox(
				&mass,
				0.7F,
				partSize.x,
				partSize.y,
				partSize.z);
		}
		else if (partInstance->shape == Enum::Shape::Cylinder)
		{
			float radius = partSize.y;
			if (partSize.z < radius)
				radius = partSize.z;
			radius *= 0.5f;
			dMassSetCylinder(
				&mass,
				0.7F,
				1,
				radius,
				partSize.x);
		}
		else
		{
			dMassSetSphere(&mass, 0.7F, partSize.x * 0.5f);
		}
		dBodySetMass(partInstance->physBody, &mass);

		// Create rigid body
		dBodySetPosition(partInstance->physBody, 
			partPosition.x,
			partPosition.y,
			partPosition.z
		);

		dGeomSetPosition(partInstance->physGeom[0], 
			partPosition.x,
			partPosition.y,
			partPosition.z);

		dBodySetLinearVel(partInstance->physBody, velocity.x, velocity.y, velocity.z);
		dBodySetAngularVel(partInstance->physBody, rotVelocity.x, rotVelocity.y, rotVelocity.z);

		Matrix3 g3dRot = partInstance->getCFrame().rotation;
		Matrix3 geomRot = g3dRot;
		if (partInstance->shape == Enum::Shape::Cylinder)
			geomRot = g3dRot *
				Matrix3::fromEulerAnglesXYZ(0, toRadians(90), 0);
		float rotation [12] = {	geomRot[0][0], geomRot[0][1], geomRot[0][2], 0,
								geomRot[1][0], geomRot[1][1], geomRot[1][2], 0,
								geomRot[2][0], geomRot[2][1], geomRot[2][2], 0};
		float bodyRotation [12] = {	g3dRot[0][0], g3dRot[0][1], g3dRot[0][2], 0,
								g3dRot[1][0], g3dRot[1][1], g3dRot[1][2], 0,
								g3dRot[2][0], g3dRot[2][1], g3dRot[2][2], 0};
		dGeomSetRotation(partInstance->physGeom[0], rotation);
		dBodySetRotation(partInstance->physBody, bodyRotation);

		if(!partInstance->isAnchored() && !partInstance->isDragging())
			dGeomSetBody(partInstance->physGeom[0], partInstance->physBody);

	} else {
		if(!partInstance->isAnchored() && !partInstance->isDragging())
		{
			const dReal* velocity = dBodyGetLinearVel(partInstance->physBody);
			const dReal* rotVelocity = dBodyGetAngularVel(partInstance->physBody);
			
			partInstance->setVelocity(Vector3(velocity[0],velocity[1],velocity[2]));
			partInstance->setRotVelocity(Vector3(rotVelocity[0],rotVelocity[1],rotVelocity[2]));

			const dReal* physPosition = dBodyGetPosition(partInstance->physBody);
			
			const dReal* physRotation = dBodyGetRotation(partInstance->physBody);
			partInstance->setCFrameNoSync(CoordinateFrame(
				Matrix3(physRotation[0],physRotation[1],physRotation[2],
						physRotation[4],physRotation[5],physRotation[6],
						physRotation[8],physRotation[9],physRotation[10]),
				Vector3(physPosition[0], physPosition[1], physPosition[2])));
		}
	}
}

void XplicitNgine::step(float stepSize)
{	
	dJointGroupEmpty(contactgroup);
	connectStuds();
	updateMotors();
	dSpaceCollide (physSpace,0,&collisionCallback);
	dWorldQuickStep(physWorld, stepSize);
}

void XplicitNgine::updateBody(PartInstance *partInstance)
{
	if(partInstance->physBody != NULL)
	{
		Vector3 position = partInstance->getCFrame().translation;

		dBodySetPosition(partInstance->physBody, 
			position[0],
			position[1],
			position[2]
		);
		dBodyEnable(partInstance->physBody);
		dGeomEnable(partInstance->physGeom[0]);

		Matrix3 g3dRot = partInstance->getCFrame().rotation;
		float rotation [12] = {	g3dRot[0][0], g3dRot[0][1], g3dRot[0][2], 0,
								g3dRot[1][0], g3dRot[1][1], g3dRot[1][2], 0,
								g3dRot[2][0], g3dRot[2][1], g3dRot[2][2], 0};

		dBodySetRotation(partInstance->physBody, rotation);
		if (partInstance->isAnchored() || partInstance->isDragging())
		{
			Matrix3 geomRot = g3dRot;
			if (partInstance->shape == Enum::Shape::Cylinder)
				geomRot = g3dRot *
					Matrix3::fromEulerAnglesXYZ(0, toRadians(90), 0);
			float geomRotation [12] = {
				geomRot[0][0], geomRot[0][1], geomRot[0][2], 0,
				geomRot[1][0], geomRot[1][1], geomRot[1][2], 0,
				geomRot[2][0], geomRot[2][1], geomRot[2][2], 0
			};
			dGeomSetRotation(partInstance->physGeom[0], geomRotation);
		}
	}
}