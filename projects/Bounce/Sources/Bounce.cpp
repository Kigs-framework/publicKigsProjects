#include "Bounce.h"
#include "FilePathManager.h"
#include "NotificationCenter.h"

using namespace Kigs;

IMPLEMENT_CLASS_INFO(Bounce);

IMPLEMENT_CONSTRUCTOR(Bounce)
{

}

void	Bounce::ProtectedInit()
{
	// Base modules have been created at this point

	// lets say that the update will sleep 1ms
	SetUpdateSleepTime(1);

	SP<File::FilePathManager> pathManager = KigsCore::Singleton<File::FilePathManager>();
	pathManager->AddToPath(".", "xml");

	// Load AppInit, GlobalConfig then launch first sequence
	DataDrivenBaseApplication::ProtectedInit();

	// init balls
	// create balls on a grid
	int currentB = 0;
	for (int i = 0; i < 10;i++)
	{
		for (int j = 0; j < 5; j++)
		{
			float r = 16.0f + (rand() % 32);
			mBalls.push_back(Ball(r,r*r));
			mBalls[currentB].SetPos({ (float)(128 + 96 * i),(float)(128 + 96 * j) });
			mBalls[currentB].SetSpeed({ (float)((rand()%513)-256),(float)((rand() % 513) - 256) });
			currentB++;
		}
	}

	// add 4 walls
	mWalls.push_back(Wall({ 0.0f,0.0f }, { 1.0,0.0 }));
	mWalls.push_back(Wall({ 0.0f,0.0f }, { 0.0,1.0 }));
	mWalls.push_back(Wall({ 1280.0f,0.0f }, { -1.0,0.0 }));
	mWalls.push_back(Wall({ 0.0f,800.0f }, { 0.0,-1.0 }));
}

void	Bounce::ProtectedUpdate()
{
	DataDrivenBaseApplication::ProtectedUpdate();

	if (mMainInterface)
	{
		if (mFirstTime < 0.0) // if first time here, init mFirstTime
		{
			mFirstTime = mApplicationTimer->GetTime();

			// and compute future collisions
			FindFutureCollisions(0.0);
		}

		// current simulation time
		double currentTime = mApplicationTimer->GetTime() - mFirstTime;

		// while collisions occurs between last simulation time and currentTime, compute new trajectories and check if other collision can occur with new trajectories
		while (computeNewTrajectories(currentTime))
		{

		}

		mPreviousTime = currentTime;

		// graphic update of balls
		for (auto& b : mBalls)
		{
			b.Update(currentTime);
		}
		// check if we want to reset all simulation
		if ((currentTime > 5.0f) && (mFutureCollisions.size())) // last reset is more than 5 second before
		{
			if ((mFutureCollisions[0].mCollisionTime - currentTime) > 0.05f) // next collision is in more than 0.05s
			{
				resetAll(currentTime);
			}
		}
	}
}

// reset all
void	Bounce::resetAll(double currentTime)
{
	mFirstTime += currentTime;
	for (auto& b : mBalls)
	{
		b.SetPos(b.GetPos(currentTime));
		b.ResetTime(0.0);
	}
	mFutureCollisions.clear();
	FindFutureCollisions(0.0);
}

// compute all possible collisions
void	Bounce::FindFutureCollisions(double time)
{
	for (int i = 0; i < mBalls.size(); i++) // for each ball
	{
		for (int j = i+1; j < mBalls.size(); j++) // check collsion with other balls
		{
			std::pair<double,double> futureC = mBalls[i].getCollisionTimeWithOther(mBalls[j]);

			double midt = (futureC.first + futureC.second) * 0.5;

			if ((midt >= time)  && (futureC.first> mPreviousTime)) // if a collision was found and collision occurs after current time
			{
				if (futureC.first < time) // need more tests
				{
					double collision_duration = (futureC.second - futureC.first);
					v2f DS(mBalls[i].GetSpeed()- mBalls[j].GetSpeed());
					DS *= collision_duration;
					float norm = length(DS);
					if ((norm < mBalls[i].GetRadius()) && (norm < mBalls[j].GetRadius())) // not a bounce, just already interpenetrating balls
					{
						break;
					}
				}

				// add this collision to future collision list
				collisionStruct toAdd = { futureC.first , &mBalls[i] ,&mBalls[j],nullptr }; // collision with two balls
				mFutureCollisions.push_back(toAdd);
			}
		}

		// check collsions with walls
		for (auto& w : mWalls)
		{
			double futureC = mBalls[i].getCollisionTimeWithWall(w);
			if (futureC >= time) // if  a collision was found and collision occurs after current time
			{
				// add this collision to future collision list
				collisionStruct toAdd = { futureC , &mBalls[i] ,nullptr,&w }; // collision with current ball and a wall
				mFutureCollisions.push_back(toAdd);
			}
		}
	}

	// sort future collision according to time
	std::sort(mFutureCollisions.begin(), mFutureCollisions.end(), [](const collisionStruct& a1, const collisionStruct& a2)
		{
			if (a1.mCollisionTime == a2.mCollisionTime)
			{
				return (a1.mBall1 < a2.mBall1);
			}
			return a1.mCollisionTime < a2.mCollisionTime;
		}
	);
}


// test if collision occurs "before" currentTime
bool	Bounce::computeNewTrajectories(double currentTime)
{
	if (mFutureCollisions.size() == 0)
	{
		return false;
	}

	if (currentTime >= mFutureCollisions[0].mCollisionTime) // a collision occured
	{
		double firstCollisionTime = mFutureCollisions[0].mCollisionTime; // get collision time
		
		int collindex = 0;
		while (mFutureCollisions[collindex].mCollisionTime <= firstCollisionTime) // in case several collisions occurs at exactly the same time, change them all
		{
			if (mFutureCollisions[collindex].mWall) // collision with wall
			{
				// set new initial pos of the ball as the collision pos 
				mFutureCollisions[collindex].mBall1->SetPos(mFutureCollisions[collindex].mBall1->GetPos(firstCollisionTime));
				// reset time of the ball ( firstCollisionTime become t0 for the ball )
				mFutureCollisions[collindex].mBall1->ResetTime(firstCollisionTime);

				// compute new ball speed
				v2f	newSpeed(mFutureCollisions[collindex].mBall1->GetSpeed());

				// compute speed symetry according to wall
				float wdot = dot(newSpeed, mFutureCollisions[collindex].mWall->GetNormal());
				newSpeed -= 2.0f * wdot * mFutureCollisions[collindex].mWall->GetNormal();

				// and set new speed
				mFutureCollisions[collindex].mBall1->SetSpeed(newSpeed);
			}
			else if (mFutureCollisions[collindex].mBall2) // collision with other ball
			{
				// set new initial pos of the ball as the collision pos for each ball
				mFutureCollisions[collindex].mBall1->SetPos(mFutureCollisions[collindex].mBall1->GetPos(firstCollisionTime));
				mFutureCollisions[collindex].mBall1->ResetTime(firstCollisionTime);
				mFutureCollisions[collindex].mBall2->SetPos(mFutureCollisions[collindex].mBall2->GetPos(firstCollisionTime));
				mFutureCollisions[collindex].mBall2->ResetTime(firstCollisionTime);

				// compute new speed for each ball
				// according to formula :
				//
				//  newspeedA = speedA -   2mB    *   Dot ( speedA - speedB , posA - posB ) * (posA-posB)  
				//                       -------      -------------------------------------
				//                      (mA + mB)               || posA-posB || ^2 

				// if DP is normalized posA-posB then formula become : 
				//
				//  newspeedA = speedA -   2mB    *   Dot ( speedA - speedB , DP ) * DP  
				//                       -------      
				//                      (mA + mB)      

				v2f	SphereSphere(mFutureCollisions[collindex].mBall2->GetPos(firstCollisionTime) - mFutureCollisions[collindex].mBall1->GetPos(firstCollisionTime));
				SphereSphere = normalize(SphereSphere);
				
				v2f sp1 = mFutureCollisions[collindex].mBall1->GetSpeed();
				v2f sp2 = mFutureCollisions[collindex].mBall2->GetSpeed();

				v2f	newSpeed(sp1);
				newSpeed -= (2.0f* mFutureCollisions[collindex].mBall2->GetMass() / (mFutureCollisions[collindex].mBall1->GetMass() + mFutureCollisions[collindex].mBall2->GetMass())) * dot(sp1 - sp2, SphereSphere) * SphereSphere;
				mFutureCollisions[collindex].mBall1->SetSpeed(newSpeed);

				newSpeed = sp2;
				newSpeed -= (2.0f * mFutureCollisions[collindex].mBall1->GetMass() / (mFutureCollisions[collindex].mBall1->GetMass() + mFutureCollisions[collindex].mBall2->GetMass())) * dot(sp2 - sp1, SphereSphere) * SphereSphere;
				
				mFutureCollisions[collindex].mBall2->SetSpeed(newSpeed);
			}

			collindex++;
		}
		// clear collision list
		mFutureCollisions.clear();
		// and recompute future collision
		FindFutureCollisions(firstCollisionTime);

		// return true so we will try again to test collisions
		return true;
	}
	// no collision occured
	return false;
}

void	Bounce::ProtectedClose()
{
	DataDrivenBaseApplication::ProtectedClose();
}

void	Bounce::ProtectedInitSequence(const std::string& sequence)
{
	if (sequence == "sequencemain")
	{
		mMainInterface = GetFirstInstanceByName("UIItem", "Interface");
		if (mMainInterface)
		{
			// set display for each ball
			int ballindex = 0;
			for (auto& b : mBalls)
			{
				std::string thumbName = "Ball_" + std::to_string(ballindex);
				CMSP toAdd = CoreModifiable::Import("ball.xml", false, false, nullptr, thumbName);
				mMainInterface->addItem(toAdd);
				b.SetUI(toAdd);
				toAdd("Color") = v3f(0.5f + ((float)(rand() % 128) / 256.0f), 0.5f + ((float)(rand() % 128) / 256.0f), 0.2f);
				toAdd("Size") = v2f(b.GetRadius() * 2.0f,b.GetRadius() * 2.0f);
				ballindex++;
			}
		}
	}
}
void	Bounce::ProtectedCloseSequence(const std::string& sequence)
{
	if (sequence == "sequencemain")
	{
		
	}
}
