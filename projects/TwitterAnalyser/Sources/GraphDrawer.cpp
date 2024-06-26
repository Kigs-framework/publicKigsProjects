#include "GraphDrawer.h"
#include "TwitterConnect.h"
#include "TwitterAnalyser.h"
#include "UI/UIImage.h"
#include "CoreFSM.h"
#include "Histogram.h"

using namespace Kigs;
using namespace Kigs::Fsm;
using namespace Kigs::File;
using namespace Kigs::Draw2D;

IMPLEMENT_CLASS_INFO(GraphDrawer)

GraphDrawer::GraphDrawer(const std::string& name, CLASS_NAME_TREE_ARG) : CoreModifiable(name, PASS_CLASS_NAME_TREE_ARG)
{

}

void GraphDrawer::InitModifiable()
{
	if (IsInit())
		return;

	CoreModifiable::InitModifiable();

	// create FSM to manage different states
	SP<CoreFSM> fsm = KigsCore::GetInstanceOf("fsm", "CoreFSM");
	// need to add fsm to the object to control
	addItem(fsm);

	mTwitterAnalyser = static_cast<TwitterAnalyser*>(KigsCore::GetCoreApplication());
	mTwitterAnalyser->AddAutoUpdate(this, 1.0);
	//mTwitterAnalyser->ChangeAutoUpdateFrequency(fsm.get(), 1.0);

	if (mTwitterAnalyser->mAnalysedType == TwitterAnalyser::dataType::TOP)
	{
		// create only TopDraw state
		fsm->addState("TopDraw", new CoreFSMStateClass(GraphDrawer, TopDraw)());

		SP<CoreFSMTransition> topdrawnexttransition = KigsCore::GetInstanceOf("topdrawnexttransition", "CoreFSMOnValueTransition");
		topdrawnexttransition->setValue("ValueName", "GoNext");
		topdrawnexttransition->setState("UserStats");
		topdrawnexttransition->Init();
		fsm->getState("TopDraw")->addTransition(topdrawnexttransition);

		// create UserStats state
		fsm->addState("UserStats", new CoreFSMStateClass(GraphDrawer, UserStats)());

		SP<CoreFSMTransition> userstatnexttransition = KigsCore::GetInstanceOf("userstatnexttransition", "CoreFSMOnValueTransition");
		userstatnexttransition->setValue("ValueName", "GoNext");
		userstatnexttransition->setState("TopDraw");
		userstatnexttransition->Init();
		fsm->getState("UserStats")->addTransition(userstatnexttransition);

		fsm->setStartState("TopDraw");
		fsm->Init();
	}
	else
	{

		// go to force state (push)
		SP<CoreFSMTransition> forcetransition = KigsCore::GetInstanceOf("forcetransition", "CoreFSMOnValueTransition");
		forcetransition->setValue("TransitionBehavior", "Push");
		forcetransition->setValue("ValueName", "DrawForce");
		forcetransition->setState("Force");
		forcetransition->Init();

		// create Percent state
		fsm->addState("Percent", new CoreFSMStateClass(GraphDrawer, Percent)());

		SP<CoreFSMTransition> percentnexttransition = KigsCore::GetInstanceOf("percentnexttransition", "CoreFSMOnValueTransition");
		percentnexttransition->setValue("ValueName", "GoNext");

		if ((mTwitterAnalyser->mPanelType == TwitterAnalyser::dataType::Followers) && (mTwitterAnalyser->mAnalysedType == TwitterAnalyser::dataType::Following))
		{
			// create Jaccard state
			fsm->addState("Jaccard", new CoreFSMStateClass(GraphDrawer, Jaccard)());

			SP<CoreFSMTransition> jaccardnexttransition = KigsCore::GetInstanceOf("jaccardnexttransition", "CoreFSMOnValueTransition");
			jaccardnexttransition->setValue("ValueName", "GoNext");
			jaccardnexttransition->setState("ReversePercent");
			jaccardnexttransition->Init();

			fsm->getState("Jaccard")->addTransition(jaccardnexttransition);
			fsm->getState("Jaccard")->addTransition(forcetransition);

			percentnexttransition->setState("Jaccard");

			// create reverse % state
			fsm->addState("ReversePercent", new CoreFSMStateClass(GraphDrawer, ReversePercent)());

			SP<CoreFSMTransition> reversenexttransition = KigsCore::GetInstanceOf("reversenexttransition", "CoreFSMOnValueTransition");
			reversenexttransition->setValue("ValueName", "GoNext");
			reversenexttransition->setState("Normalized");
			reversenexttransition->Init();

			fsm->getState("ReversePercent")->addTransition(reversenexttransition);
			fsm->getState("ReversePercent")->addTransition(forcetransition);

		}
		else
		{
			percentnexttransition->setState("Normalized");
		}

		percentnexttransition->Init();

		fsm->getState("Percent")->addTransition(percentnexttransition);
		fsm->getState("Percent")->addTransition(forcetransition);

		// create Normalized state
		fsm->addState("Normalized", new CoreFSMStateClass(GraphDrawer, Normalized)());
		fsm->getState("Normalized")->addTransition(forcetransition);
		SP<CoreFSMTransition> normalizednexttransition = KigsCore::GetInstanceOf("normalizednexttransition", "CoreFSMOnValueTransition");
		normalizednexttransition->setValue("ValueName", "GoNext");

		if ((mTwitterAnalyser->mPanelType == TwitterAnalyser::dataType::Following) && (mTwitterAnalyser->mAnalysedType == TwitterAnalyser::dataType::Interactors))
		{
			normalizednexttransition->setState("OpeningMeasurement");
			// create Opening and Closing state
			fsm->addState("OpeningMeasurement", new CoreFSMStateClass(GraphDrawer, OpeningMeasurement)());
			fsm->addState("ClosingMeasurement", new CoreFSMStateClass(GraphDrawer, ClosingMeasurement)());
			fsm->addState("OpeningClosingStats", new CoreFSMStateClass(GraphDrawer, OpeningClosingStats)());

			SP<CoreFSMTransition> openingnexttransition = KigsCore::GetInstanceOf("openingnexttransition", "CoreFSMOnValueTransition");
			openingnexttransition->setValue("ValueName", "GoNext");
			openingnexttransition->setState("ClosingMeasurement");
			openingnexttransition->Init();

			fsm->getState("OpeningMeasurement")->addTransition(openingnexttransition);
			
			SP<CoreFSMTransition> closingnexttransition = KigsCore::GetInstanceOf("closingnexttransition", "CoreFSMOnValueTransition");
			closingnexttransition->setValue("ValueName", "GoNext");
			closingnexttransition->setState("OpeningClosingStats");
			closingnexttransition->Init();

			fsm->getState("ClosingMeasurement")->addTransition(closingnexttransition);


			SP<CoreFSMTransition> opclostatsnexttransition = KigsCore::GetInstanceOf("opclostatsnexttransition", "CoreFSMOnValueTransition");
			opclostatsnexttransition->setValue("ValueName", "GoNext");
			opclostatsnexttransition->setState("UserStats");
			opclostatsnexttransition->Init();

			fsm->getState("OpeningClosingStats")->addTransition(opclostatsnexttransition);
		}
		else
		{
			
			normalizednexttransition->setState("UserStats");
			
		}
		fsm->getState("Normalized")->addTransition(normalizednexttransition);
		normalizednexttransition->Init();


		// create UserStats state
		fsm->addState("UserStats", new CoreFSMStateClass(GraphDrawer, UserStats)());

		SP<CoreFSMTransition> userstatnexttransition = KigsCore::GetInstanceOf("userstatnexttransition", "CoreFSMOnValueTransition");
		userstatnexttransition->setValue("ValueName", "GoNext");
		userstatnexttransition->setState("Percent");
		userstatnexttransition->Init();
		fsm->getState("UserStats")->addTransition(userstatnexttransition);

		// pop wait state transition
		SP<CoreFSMTransition> forceendtransition = KigsCore::GetInstanceOf("forceendtransition", "CoreFSMOnValueTransition");
		forceendtransition->setValue("TransitionBehavior", "Pop");
		forceendtransition->setValue("ValueName", "DrawForce");
		forceendtransition->setValue("NotValue", true); // end wait when NeedWait is false
		forceendtransition->Init();

		// create Force state
		fsm->addState("Force", new CoreFSMStateClass(GraphDrawer, Force)());
		fsm->getState("Force")->addTransition(forceendtransition);

		fsm->setStartState("Percent");
		fsm->Init();
	}

}

void	GraphDrawer::drawSpiral(std::vector<std::tuple<unsigned int,float, u64> >&	toShow)
{
	drawGeneralStats();
	if (mTwitterAnalyser->mValidUserCount < 10)
		return;

	int toShowCount = 0;
	float dangle = 2.0f * fPI / 7.0f;
	float angle = 0.0f;
	float ray = 0.15f;
	float dray = 0.0117f;
	for (const auto& toS : toShow)
	{
		if( (std::get<2>(toS) == mTwitterAnalyser->mPanelRetreivedUsers.getUserStructAtIndex(0).mID) && !mShowMyself)
		{
			continue;
		}

		auto& toPlace = mTwitterAnalyser->mInStatsUsers[std::get<2>(toS)];

		auto found = mShowedUser.find(std::get<2>(toS));
		if (found != mShowedUser.end())
		{
			const CMSP& toSetup = (*found).second;
			v2f dock(0.53f + ray * cosf(angle), 0.47f + ray * 1.02f * sinf(angle));
			toSetup("Dock") = dock;

			angle += dangle;
			dangle = 2.0f * fPI / (2.0f + 50.0f * ray);
			ray += dray;
			dray *= 0.98f;
			if (toPlace.second.mName.length())
			{
				toSetup["ChannelName"]("Text") = toPlace.second.mName;
			}
			else
			{
				if (!TwitterConnect::LoadUserStruct(std::get<2>(toS), toPlace.second, true))
				{
					mTwitterAnalyser->askUserDetail(std::get<2>(toS));
				}
			}

			float prescale = 1.0f;

			int score = (int)std::get<1>(toS);
			std::string scorePrint;

			if (mCurrentUnit == AnonymousCount)
			{
				scorePrint = std::to_string(std::get<0>(toS));
			}
			else
			{
				scorePrint = std::to_string(score);

			}

			scorePrint+= mUnits[mCurrentUnit];

			toSetup["ChannelPercent"]("Text") = scorePrint;

			prescale = score / 80.0f;
			
			prescale = sqrtf(prescale);
			if (prescale > 1.0f)
			{
				prescale = 1.0f;
			}

			prescale *= 1.2f;

			// set ChannnelPercent position depending on where the thumb is in the spiral

			if ((dock.x > 0.45f) && (dock.x < 0.61f))
			{
				toSetup["ChannelPercent"]("Dock") = v2f(0.5, 0.0);
				toSetup["ChannelPercent"]("Anchor") = v2f(0.5, 1.0);
			}
			else if (dock.x <= 0.45f)
			{
				toSetup["ChannelPercent"]("Dock") = v2f(1.0, 0.5);
				toSetup["ChannelPercent"]("Anchor") = v2f(0.0, 0.5);
			}
			else
			{
				toSetup["ChannelPercent"]("Dock") = v2f(0.0, 0.5);
				toSetup["ChannelPercent"]("Anchor") = v2f(1.0, 0.5);
			}

			toSetup("PreScale") = v2f(1.44f * prescale, 1.44f * prescale);

			toSetup("Radius") = ((v2f)toSetup("Size")).x * 1.44f * prescale * 0.5f;

			toSetup["ChannelName"]("PreScale") = v2f(1.0f / (1.44f * prescale), 1.0f / (1.44f * prescale));

			toSetup["ChannelPercent"]("PreScale") = v2f(0.8f, 0.8f);
			toSetup["ChannelPercent"]("FontSize") = 0.6f * 24.0f / prescale;
			toSetup["ChannelPercent"]("MaxWidth") = 0.8f * 100.0f / prescale;
				
			toSetup["ChannelName"]("FontSize") = 20.0f;
			toSetup["ChannelName"]("MaxWidth") = 160.0f;

			const SP<UIImage>& checkTexture = toSetup;

			if (toPlace.second.mName.length())
			{
				if (!checkTexture->HasTexture())
				{
					//somethingChanged = true;
					if (toPlace.second.mThumb.mTexture)
					{
						checkTexture->addItem(toPlace.second.mThumb.mTexture);
					}
					else
					{
						TwitterConnect::LoadUserStruct(std::get<2>(toS), toPlace.second, true);
					}
				}
			}

			if (toShowCount >= mTwitterAnalyser->mMaxUserCount)
			{
				dock = v2f(-5.0f, -5.0f);
				toSetup("Dock") = dock;
			}

		}
		toShowCount++;
		
	}
	
}

void	GraphDrawer::prepareForceGraphData()
{

	Utils::Histogram<float>	hist({ 0.0,1.0 }, 256);

	mAccountSubscriberMap.clear();

	// for each showed channel
	for (auto& c : mShowedUser)
	{
		const auto& UserStatsList=mTwitterAnalyser->mPerPanelUsersStats;
		TwitterConnect::PerAccountUserMap	toAdd(UserStatsList.size());
		int sindex = 0;
		int subcount = 0;
		for (auto& s : UserStatsList)
		{
			if (mTwitterAnalyser->isUserOf(s.first, c.first))
			{
				toAdd.SetSubscriber(sindex);
			}
			++sindex;
		}
		toAdd.mThumbnail = c.second;
		c.second["ChannelPercent"]("Text") = "";
		c.second["ChannelName"]("Text") = "";
		mAccountSubscriberMap[c.first] = toAdd;
	}

	for (auto& l1 : mAccountSubscriberMap)
	{
		l1.second.mPos = l1.second.mThumbnail->getValue<v2f>("Dock");
		l1.second.mPos.x = 960 + (rand() % 129) - 64;
		l1.second.mPos.y = 540 + (rand() % 81) - 40;
		l1.second.mForce = v2f(0.0f, 0.0f);
		l1.second.mRadius = l1.second.mThumbnail->getValue<float>("Radius");

		int index = 0;
		for (auto& l2 : mAccountSubscriberMap)
		{
			if (&l1 == &l2)
			{
				l1.second.mCoeffs.push_back({ index,-1.0f });
			}
			else
			{
				float coef = l1.second.GetNormalisedSimilitude(l2.second);
				l1.second.mCoeffs.push_back({ index ,coef });
				hist.addValue(coef);
			}
			++index;
		}

		// sort mCoeffs by value
		std::sort(l1.second.mCoeffs.begin(), l1.second.mCoeffs.end(), [](const std::pair<int, float>& a1, const std::pair<int, float>& a2)
			{
				if (a1.second == a2.second)
				{
					return (a1.first < a2.first);
				}

				return a1.second < a2.second;
			}
		);


		// sort again mCoeffs but by index 
		std::sort(l1.second.mCoeffs.begin(), l1.second.mCoeffs.end(), [](const std::pair<int, float>& a1, const std::pair<int, float>& a2)
			{
				return a1.first < a2.first;
			}
		);
	}

	hist.normalize();

	std::vector<float> histlimits = hist.getPercentList({ 0.05f,0.5f,0.96f });


	float rangecoef1 = 1.0f / (histlimits[1] - histlimits[0]);
	float rangecoef2 = 1.0f / (histlimits[2] - histlimits[1]);
	int index1 = 0;
	for (auto& l1 : mAccountSubscriberMap)
	{
		// recompute coeffs according to histogram
		int index2 = 0;
		for (auto& c : l1.second.mCoeffs)
		{
			if (index1 != index2)
			{
				// first half ?
				if ((c.second >= histlimits[0]) && (c.second <= histlimits[1]))
				{
					c.second -= histlimits[0];
					c.second *= rangecoef1;
					c.second -= 1.0f;
					c.second = c.second * c.second * c.second;
					c.second += 1.0f;
					c.second *= (histlimits[1] - histlimits[0]);
					c.second += histlimits[0];
				}
				else if ((c.second > histlimits[1]) && (c.second <= histlimits[2]))
				{
					c.second -= histlimits[1];
					c.second *= rangecoef2;
					c.second = c.second * c.second * c.second;
					c.second *= (histlimits[2] - histlimits[1]);
					c.second += histlimits[1];
				}
				c.second -= histlimits[1];
				c.second *= 2.0f;

			}
			++index2;
		}
		++index1;
	}
}

void	GraphDrawer::drawForce()
{
	
	v2f center(1920.0 / 2.0, 1080.0 / 2.0);

	const float timeDelay = 10.0f;
	const float timeDivisor = 0.04f;

	float currentTime = ((KigsCore::GetCoreApplication()->GetApplicationTimer()->GetTime() - mForcedBaseStartingTime) - timeDelay) * timeDivisor;
	if (currentTime > 1.0f)
	{
		currentTime = 1.0f;
	}

	// first compute attraction force on each thumb
	for (auto& l1 : mAccountSubscriberMap)
	{
		TwitterConnect::PerAccountUserMap& current = l1.second;

		// always a  bit of attraction to center
		v2f	v(center);
		v -= current.mPos;

		// add a bit of random
		v.x += (rand() % 32) - 16;
		v.y += (rand() % 32) - 16;

		current.mForce *= 0.1f;
		current.mForce = v * 0.001f;

		int i = 0;
		for (auto& l2 : mAccountSubscriberMap)
		{
			if (&l1 == &l2)
			{
				i++;
				continue;
			}

			v2f	v(l2.second.mPos - current.mPos);
			float dist = length(v);
			v = normalize(v);
			float coef = current.mCoeffs[i].second;
			if (currentTime < 0.0f)
			{
				coef += -currentTime / (timeDelay * timeDivisor);
			}
			if (coef > 0.0f)
			{
				if (dist <= (current.mRadius + l2.second.mRadius)) // if not already touching
				{
					coef = 0.0f;
				}
			}

			current.mForce += v * coef * (l2.second.mRadius + current.mRadius) / ((l2.second.mRadius + current.mRadius) + (dist * dist) * 0.001);

			i++;
		}
	}
	// then move according to forces
	for (auto& l1 : mAccountSubscriberMap)
	{
		TwitterConnect::PerAccountUserMap& current = l1.second;
		current.mPos += current.mForce;
	}

	if (currentTime > 0.0f)
	{
		// then adjust position with contact
		for (auto& l1 : mAccountSubscriberMap)
		{
			TwitterConnect::PerAccountUserMap& current = l1.second;

			for (auto& l2 : mAccountSubscriberMap)
			{
				if (&l1 == &l2)
				{
					continue;
				}

				v2f	v(l2.second.mPos - current.mPos);
				float dist = length(v);
				v = normalize(v);

				float r = (current.mRadius + l2.second.mRadius) * currentTime;
				if (dist < r)
				{
					float coef = (r - dist) * 0.5f;
					current.mPos -= v * coef;
					l2.second.mPos += v * coef;
				}
			}
		}
	}
	// then check if they don't get out of the screen
	for (auto& cl1 : mAccountSubscriberMap)
	{

		TwitterConnect::PerAccountUserMap& current = cl1.second;
		v2f	v(current.mPos);
		v -= center;
		float l1 = length(v);

		float angle = atan2f(v.y, v.x);
		v = normalize(v);

		v2f ellipse(center);
		ellipse.x *= 0.9 * cosf(angle);
		ellipse.y *= 0.9 * sinf(angle);

		v2f tst = normalize(ellipse);

		ellipse -= tst * current.mRadius;
		float l2 = length(ellipse);

		if (l1 > l2)
		{
			current.mPos -= v * (l1 - l2);
		}

		v2f dock(current.mPos);
		dock.x /= 1920.0f;
		dock.y /= 1080.0f;

		current.mThumbnail("Dock") = dock;
	}

	
}

template<typename T>
void	GraphDrawer::Diagram::Draw(const std::vector<T>& values)
{
	KigsBitmap::KigsBitmapPixel	black(0, 0, 0, 255);
	// print title
	mBitmap->Print(mTitle, mZonePos.x + mZoneSize.x / 2.0f, mZonePos.y, 1, mZoneSize.x, 32, "Calibri.ttf", 1, black);

	std::vector<T> sortedV = values;
	std::sort(sortedV.begin(), sortedV.end());
	
	float minv = sortedV[0];
	float maxv = sortedV.back();

	// given limits ?
	if (mLimits.x < mLimits.y)
	{
		minv = mLimits.x;
		maxv = mLimits.y;
	}

	// apply multiplier first
	if (mMultiplier)
	{
		minv *= mMultiplier;
		maxv *= mMultiplier;
	}
	// then shift
	if (mShift)
	{
		minv += mShift;
		maxv += mShift;
	}

	if (mUseLog)
	{
		if (minv < 1.0)
		{
			minv = 1.0f;
		}
		minv = log10f(minv);
		maxv = log10f(maxv);
	}

	float average = 0.0f;

	Utils::Histogram<float>	hist({ minv,maxv }, mColumnCount);
	for (auto v : values)
	{
		float transformV=v;

		// apply multiplier first
		if (mMultiplier)
		{
			transformV *= mMultiplier;
		}
		// then shift
		if (mShift)
		{
			transformV += mShift;
		}

		if (mUseLog)
		{
			if (transformV < 1.0)
				transformV = 1.0f;
			transformV = log10f(transformV);
		}
		
		hist.addValue(transformV);
		average += v;
	}

	average /= (float)(values.size());

	// keep some space for title, axis...
	v2i	DiagramSize = mZoneSize;
	DiagramSize -= v2i(64, 64);

	v2i DiagramPos = mZonePos;
	DiagramPos += v2i(48, 32);

	hist.normalize();

	u32 higherColumn=hist.getMaxValueColumnIndex();
	u32 RoundedHigherPercent = (u32)(hist.getColumnValue(higherColumn)*100.0f+9.99f);
	RoundedHigherPercent /= 10;
	RoundedHigherPercent *= 10;

	float columnSizeCoef = 1.0f / (0.01f*(float)RoundedHigherPercent);
	
	int columnSizeX = DiagramSize.x / mColumnCount;
	columnSizeX -= 4;
	if (columnSizeX < 10)
	{
		columnSizeX = 10;
	}

	int columnSpaceX = (DiagramSize.x / mColumnCount) - columnSizeX;

	for (u32 i = 0; i < mColumnCount; i++)
	{
		int columnSizeY = (float)DiagramSize.y * hist.getColumnValue(i) * columnSizeCoef;
		
		mBitmap->Box(columnSpaceX/2+DiagramPos.x + (float)i * (float)DiagramSize.x / (float)mColumnCount, DiagramPos.y + DiagramSize.y- columnSizeY, columnSizeX, columnSizeY, mColumnColor);
	}

	// print percents
	for (u32 i = 0; i <= RoundedHigherPercent; i += 10)
	{
		u32 linePosY = DiagramPos.y + DiagramSize.y - (float)i * 0.01f * columnSizeCoef * DiagramSize.y;
		mBitmap->Line(DiagramPos.x-1, linePosY, DiagramPos.x + DiagramSize.x+1, linePosY, { 0,0,0,128 });
		std::string printedPercent = std::to_string(i) + "%";
		mBitmap->Print(printedPercent, DiagramPos.x - 16, linePosY - 9, 1, 48, 18, "Calibri.ttf", 1, black);
	}

	// Y axis
	mBitmap->Line(DiagramPos.x, DiagramPos.y-1, DiagramPos.x, DiagramPos.y + DiagramSize.y +1, { 0,0,0,128 });

	// avegare line
	//
	{
		std::string printedLegend;

		if (mShift || mMultiplier)
		{
			char buffer[20];  // maximum expected length of the float
			std::snprintf(buffer, 20, "%.1f", average);
			printedLegend = buffer;
		}
		else
		{
			printedLegend = std::to_string((u32)average);
		}

		// apply multiplier first
		if (mMultiplier)
		{
			average *= mMultiplier;
		}
		// then shift
		if (mShift)
		{
			average += mShift;
		}

		if (mUseLog)
		{
			if (average < 1.0)
				average = 1.0f;

			average = log10f(average);
		}
		

		float averagePos = (float)DiagramPos.x + (float)DiagramSize.x * (average - minv) / (maxv - minv);
		if (averagePos < (float)DiagramPos.x)
		{
			averagePos = (float)DiagramPos.x;
		}
		else if (averagePos > (float)(DiagramPos.x + DiagramSize.x))
		{
			averagePos = (float)(DiagramPos.x + DiagramSize.x);
		}
		mBitmap->Line(averagePos, DiagramPos.y - 1, averagePos, DiagramPos.y + DiagramSize.y + 14, { 255,0,0,128 });
		mBitmap->Print(printedLegend, averagePos, DiagramPos.y + DiagramSize.y + 14, 1, 96, 16, "Calibri.ttf", 1, { 128,0,0,128 });
	}
	// median line
	//
	{
		std::string printedLegend;
		float median = sortedV[sortedV.size()/2];

		if (mShift || mMultiplier)
		{
			char buffer[20];  // maximum expected length of the float
			std::snprintf(buffer, 20, "%.1f", median);
			printedLegend = buffer;
		}
		else
		{
			printedLegend = std::to_string((u32)median);
		}
		// apply multiplier first
		if (mMultiplier)
		{
			median *= mMultiplier;
		}
		// then shift
		if (mShift)
		{
			median += mShift;
		}

		if (mUseLog)
		{
			
			if (median < 1.0)
				median = 1.0f;

			median = log10f(median);
		}
		
		float medianPos = (float)DiagramPos.x + (float)DiagramSize.x * (median - minv) / (maxv - minv);
		if (medianPos < (float)DiagramPos.x)
		{
			medianPos = (float)DiagramPos.x;
		}
		else if (medianPos > (float)(DiagramPos.x + DiagramSize.x))
		{
			medianPos = (float)(DiagramPos.x + DiagramSize.x);
		}
		mBitmap->Line(medianPos, DiagramPos.y - 1, medianPos, DiagramPos.y + DiagramSize.y + 24, { 0,200,0,128 });
		mBitmap->Print(printedLegend, medianPos, DiagramPos.y + DiagramSize.y + 26, 1, 96, 16, "Calibri.ttf", 1, { 0,128,0,128 });
	}

	// X legend
	if (mUseLog)
	{
		for (u32 i = 1; i < mColumnCount; i++)
		{
			u32 printPosX = DiagramPos.x + (float)i * (float)DiagramSize.x / (float)mColumnCount;

			u32 legend = 1 + pow(10.f, minv + (float)i * ((maxv - minv) / (float)mColumnCount));

			float transformedLegend = legend;
			// invert shift / multiplier
			if (mShift)
			{
				transformedLegend -= mShift;
			}
			if (mMultiplier)
			{
				transformedLegend /= mMultiplier;
			}

			std::string printedLegend;

			if (mShift || mMultiplier)
			{
				char buffer[20];  // maximum expected length of the float
				std::snprintf(buffer, 20, "%.1f", transformedLegend);
				printedLegend=buffer;
			}
			else
			{
				printedLegend = std::to_string(legend);
			}

			if (i == mColumnCount - 1)
				printedLegend += " & more";

			mBitmap->Print(printedLegend, printPosX, DiagramPos.y + DiagramSize.y + 2, 1, 96, 12, "Calibri.ttf", 0, black);
		}
	}
	else
	{
		for (u32 i = 0; i <= mColumnCount; i++)
		{
			u32 printPosX = DiagramPos.x + (float)i * (float)DiagramSize.x / (float)mColumnCount;
			float legend = minv + (float)i * ((maxv - minv) / (float)mColumnCount);
			
			// invert shift / multiplier
			if (mShift)
			{
				legend -= mShift;
			}
			if (mMultiplier)
			{
				legend /= mMultiplier;
			}

			char buffer[20];  // maximum expected length of the float
			std::snprintf(buffer, 20, "%.1f", legend);
			std::string printedLegend(buffer);

			mBitmap->Print(printedLegend, printPosX, DiagramPos.y + DiagramSize.y + 2, 1, 96, 12, "Calibri.ttf", 1, black);
		}
	}
}

void	GraphDrawer::drawStats(SP<KigsBitmap> bitmap)
{
	drawGeneralStats();
	bitmap->Clear({ 0,0,0,0 });

	// title
	bitmap->Print("Sample statistics", 1920 / 2, 16, 1, 1920, 92, "Calibri.ttf", 1, {0,0,0,128});

	// count twitter blue
	size_t twitterBlueCount = 0;
	// followers
	std::vector<float> currentData;
	for (auto u : mTwitterAnalyser->mPerPanelUsersStats)
	{
		const auto& userdata = mTwitterAnalyser->getRetreivedUser(u.first);
		if (userdata.mFollowersCount + userdata.mFollowingCount)
		{
			currentData.push_back(userdata.mFollowersCount);
			if (userdata.mTwitterBlue)
			{
				twitterBlueCount++;
			}
		}
	}

	std::string panelsize = "(sample size = " + std::to_string(currentData.size()) + ")";
	bitmap->Print(panelsize, 1920 / 2, 100, 1, 1920, 24, "Calibri.ttf", 1, { 0,0,0,128 });

	// print twitter blue %
	if (currentData.size())
	{
		std::string twitterBluePercent = "Twitter Blue percent = " + std::to_string((twitterBlueCount * 100) / currentData.size()) + "%";
		bitmap->Print(twitterBluePercent, 1920 / 2, 120, 1, 1920, 16, "Calibri.ttf", 1, { 0,0,0,128 });
	}


	Diagram	diagram(bitmap);
	diagram.mZonePos = v2i(64, 256);
	diagram.mColumnCount = 10;
	diagram.mZoneSize = v2i(512, 288);
	diagram.mLimits = v2f( 10.0f, 10000.0f );
	diagram.mUseLog = true;
	diagram.mTitle = "Followers Count";
	diagram.mColumnColor = { 94,169,221,255 };
	if (currentData.size())
	{
		diagram.Draw(currentData);
	}
	
	// followings
	currentData.clear();
	for (auto u : mTwitterAnalyser->mPerPanelUsersStats)
	{
		const auto& userdata = mTwitterAnalyser->getRetreivedUser(u.first);
		if (userdata.mFollowersCount + userdata.mFollowingCount)
		{
			currentData.push_back(userdata.mFollowingCount);
		}
	}

	diagram.mZonePos = v2i(1920/2-256, 256);
	diagram.mColumnCount = 8;
	diagram.mZoneSize = v2i(512, 288);
	diagram.mLimits = v2f(10.0f, 5000.0f);
	diagram.mUseLog = true;
	diagram.mTitle = "Following Count";
	diagram.mColumnColor = { 94,169,221,255 };
	if (currentData.size())
	{
		diagram.Draw(currentData);
	}

	// user account "age"
	currentData.clear();

	std::vector<float>	activityIndice;
	std::vector<float>	followerperage;

	std::string endDate = TwitterConnect::getTodayDate();
	if (TwitterConnect::useDates())
	{
		std::string endDate = TwitterConnect::getDate(1) + "T00:00:00.000Z";
	}
	
	for (auto u : mTwitterAnalyser->mPerPanelUsersStats)
	{
		const auto& userdata = mTwitterAnalyser->getRetreivedUser(u.first);
		if (userdata.mFollowersCount + userdata.mFollowingCount)
		{
			std::string creationDate = userdata.UTCTime;
		
			int age= TwitterConnect::getDateDiffInMonth(creationDate, endDate);

			activityIndice.push_back((float)userdata.mStatuses_count / (float)(age + 1));
			followerperage.push_back((float)userdata.mFollowersCount / (float)(age + 1));
			currentData.push_back(age);
		}
		
	}

	diagram.mZonePos = v2i(64, 512+64);
	diagram.mColumnCount = 8;
	diagram.mZoneSize = v2i(512, 288);
	diagram.mLimits = v2f(2.0f, 120.0f);
	diagram.mUseLog = true;
	diagram.mTitle = "Account ages in months";
	diagram.mColumnColor = { 94,169,221,255 };
	if (currentData.size())
	{
		diagram.Draw(currentData);
	}

	// follower per tweet ratio
	currentData.clear();

	for (auto u : mTwitterAnalyser->mPerPanelUsersStats)
	{
		const auto& userdata = mTwitterAnalyser->getRetreivedUser(u.first);
		if (userdata.mFollowersCount + userdata.mFollowingCount)
		{
			float ratio = (float)userdata.mFollowersCount / (float)(userdata.mStatuses_count+1);
			currentData.push_back(ratio);
		}
	}

	diagram.mZonePos = v2i(1920 / 2 - 256, 512 + 64);
	diagram.mColumnCount = 10;
	diagram.mZoneSize = v2i(512, 288);
	diagram.mLimits = v2f(0.1f, 2.0f);
	diagram.mShift=1.0f;
	diagram.mMultiplier=10.0f;
	diagram.mUseLog = true;
	diagram.mTitle = "Follower per tweet ratio";
	diagram.mColumnColor = { 94,169,221,255 };
	if (currentData.size())
	{
		diagram.Draw(currentData);
	}


	diagram.mShift=0.0f;
	diagram.mMultiplier=0.0f;
	// activity indice
	diagram.mZonePos = v2i(1920 - 64 - 512, 512+64);
	diagram.mColumnCount = 10;
	diagram.mZoneSize = v2i(512, 288);
	diagram.mLimits = v2f(10.0f, 2000.0f);
	diagram.mUseLog = true;
	diagram.mTitle = "Average activity per month";
	diagram.mColumnColor = { 94,169,221,255 };
	if (activityIndice.size())
	{
		diagram.Draw(activityIndice);
	}

	// follower per month
	diagram.mZonePos = v2i(1920 - 64 - 512, 256);
	diagram.mColumnCount = 10;
	diagram.mZoneSize = v2i(512, 288);
	diagram.mLimits = v2f(0.0f, 50.0f);
	diagram.mShift = 1.0f;
	diagram.mMultiplier = 2.0f;
	diagram.mUseLog = true;
	diagram.mTitle = "Average follower per month";
	diagram.mColumnColor = { 94,169,221,255 };
	if (followerperage.size())
	{
		diagram.Draw(followerperage);
	}

}

void	GraphDrawer::nextDrawType()
{
	mGoNext = true;
}

void	GraphDrawer::drawGeneralStats()
{
	char textBuffer[256];
	sprintf(textBuffer, "Treated %s : %d", mTwitterAnalyser->PanelUserName[(size_t)mTwitterAnalyser->mPanelType].c_str(), mTwitterAnalyser->mTreatedUserCount);
	
	mMainInterface["TreatedFollowers"]("Text") = textBuffer;

	sprintf(textBuffer, "%s %s count : %d", mTwitterAnalyser->PanelUserName[(size_t)mTwitterAnalyser->mPanelType].c_str(),
												mTwitterAnalyser->PanelUserName[(size_t)mTwitterAnalyser->mAnalysedType].c_str(),
												(int)mTwitterAnalyser->mInStatsUsers.size());
	
	mMainInterface["FoundFollowings"]("Text") = textBuffer;

	sprintf(textBuffer, "Invalid %s count : %d", mTwitterAnalyser->PanelUserName[(size_t)mTwitterAnalyser->mPanelType].c_str(), mTwitterAnalyser->mTreatedUserCount - mTwitterAnalyser->mValidUserCount);
	
	mMainInterface["FakeFollowers"]("Text") = textBuffer;

	if (!mEverythingDone)
	{
		sprintf(textBuffer, "Twitter API requests : %d", mTwitterAnalyser->mTwitterConnect->getRequestCount());
		mMainInterface["RequestCount"]("Text") = textBuffer;

		if (mTwitterAnalyser->mTwitterConnect->mWaitQuota)
		{
			sprintf(textBuffer, "Wait quota count: %d", mTwitterAnalyser->mTwitterConnect->mWaitQuotaCount);
		}
		else
		{
			double requestWait = mTwitterAnalyser->mTwitterConnect->getDelay();
			if (requestWait < 0.0)
			{
				requestWait = 0.0;
			}
			sprintf(textBuffer, "Next request in : %f", requestWait);
		}


		mMainInterface["RequestWait"]("Text") = textBuffer;
	}
	else
	{
		if (TwitterConnect::useDates())
		{
			mMainInterface["RequestCount"]("Text") = "From: " + TwitterConnect::getDate(0) + "  To: " + TwitterConnect::getDate(1);
		}
		else
		{
			mMainInterface["RequestCount"]("Text") = "";
		}
		mMainInterface["RequestWait"]("Text") = ""; 
		
		mMainInterface["switchForce"]("IsHidden") = !mCurrentStateHasForceDraw;
		mMainInterface["switchForce"]("IsTouchable") = mCurrentStateHasForceDraw;
		
	}

	if (mTwitterAnalyser->mPanelRetreivedUsers.getUserStructAtIndex(0).mThumb.mTexture && mMainInterface["thumbnail"])
	{
		const SP<UIImage>& tmp = mMainInterface["thumbnail"];

		if (!tmp->HasTexture())
		{
			tmp->addItem(mTwitterAnalyser->mPanelRetreivedUsers.getUserStructAtIndex(0).mThumb.mTexture);

			usString	thumbname = mTwitterAnalyser->mPanelRetreivedUsers.getUserStructAtIndex(0).mName;

			if (mTwitterAnalyser->mUseHashTags)
			{
				thumbname = std::string(" ");
				thumbname +=mTwitterAnalyser->mPanelRetreivedUsers.getUserStructAtIndex(0).mName;
			}
			mMainInterface["thumbnail"]["UserName"]("Text") = thumbname;
		}

	}
	else if (mMainInterface["thumbnail"])
	{
		TwitterConnect::LoadUserStruct(mTwitterAnalyser->mPanelRetreivedUsers.getUserStructAtIndex(0).mID, mTwitterAnalyser->mPanelRetreivedUsers.getUserStructAtIndex(0), true);
	}
}


void CoreFSMStartMethod(GraphDrawer, Percent)
{
	mGoNext = false;
	mCurrentUnit = 0;
	mCurrentStateHasForceDraw = true;
}

void CoreFSMStopMethod(GraphDrawer, Percent)
{
}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(GraphDrawer, Percent))
{
	float wantedpercent = mTwitterAnalyser->mValidUserPercent;

	std::vector<std::tuple<unsigned int,float, u64>>	toShow;
	for (auto c : mTwitterAnalyser->mInStatsUsers)
	{
		if (c.first != mTwitterAnalyser->mPanelRetreivedUsers.getUserStructAtIndex(0).mID)
		{
			if (c.second.first > 3)
			{
				float percent = (float)c.second.first / (float)mTwitterAnalyser->mValidUserCount;
				if (percent > wantedpercent)
				{
					toShow.push_back({ c.second.first,percent*100.0f,c.first });
				}
			}
		}
	}

	if (toShow.size() == 0)
	{
		drawGeneralStats();
		return false;
	}
	std::sort(toShow.begin(), toShow.end(), [&](const std::tuple<unsigned int, float,u64>& a1, const std::tuple<unsigned int, float,u64>& a2)
		{
			if (std::get<0>(a1) == std::get<0>(a2))
			{
				return std::get<2>(a1) > std::get<2>(a2);
			}
			return (std::get<0>(a1) > std::get<0>(a2));
		}
	);

	std::unordered_map<u64, unsigned int>	currentShowedChannels;
	int toShowCount = 0;
	for (const auto& tos : toShow)
	{
		currentShowedChannels[std::get<2>(tos)] = 1;
		toShowCount++;

		const auto& a1User = mTwitterAnalyser->mInStatsUsers[std::get<2>(tos)];

		if (toShowCount >= (mTwitterAnalyser->mMaxUserCount*3))
			break;
	}

	for (const auto& s : mShowedUser)
	{
		if (currentShowedChannels.find(s.first) == currentShowedChannels.end())
		{
			currentShowedChannels[s.first] = 0;
		}
		else
		{
			currentShowedChannels[s.first]++;
		}
	}

	// add / remove items
	for (const auto& update : currentShowedChannels)
	{
		if (update.second == 0) // to remove 
		{
			auto toremove = mShowedUser.find(update.first);
			mMainInterface->removeItem((*toremove).second);
			mShowedUser.erase(toremove);
			//somethingChanged = true;
		}
		else if (update.second == 1) // to add
		{
			std::string thumbName = "thumbNail_" + TwitterConnect::GetIDString(update.first);
			CMSP toAdd = CoreModifiable::Import("Thumbnail.xml", false, false, nullptr, thumbName);
			toAdd->AddDynamicAttribute<maFloat, float>("Radius", 1.0f);
			mShowedUser[update.first] = toAdd;
			mMainInterface->addItem(toAdd);
			//somethingChanged = true;
		}
	}

	drawSpiral(toShow);
	return false;
}


void CoreFSMStartMethod(GraphDrawer, Jaccard)
{
	mGoNext = false;
	mCurrentUnit = 1;
	mCurrentStateHasForceDraw = true;
}

void CoreFSMStopMethod(GraphDrawer, Jaccard)
{
}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(GraphDrawer, Jaccard))
{
	float wantedpercent = mTwitterAnalyser->mValidUserPercent;

	auto getData = [&](const TwitterConnect::UserStruct& strct)->u32
	{
		if (mTwitterAnalyser->mPanelType == TwitterAnalyser::dataType::Followers)
		{
			return strct.mFollowersCount;
		}
		return strct.mFollowingCount;
	};

	std::vector<std::tuple<unsigned int, float, u64>>	toShow;
	for (auto c : mTwitterAnalyser->mInStatsUsers)
	{
		if (c.first != mTwitterAnalyser->mPanelRetreivedUsers.getUserStructAtIndex(0).mID)
		{
			if (c.second.first > 3)
			{
				float percent = (float)c.second.first / (float)mTwitterAnalyser->mValidUserCount;
				if (percent > wantedpercent)
				{
					const auto& a1User = mTwitterAnalyser->mInStatsUsers[c.first];
					if (a1User.second.mName.ToString() == "") // not loaded
					{
						mTwitterAnalyser->askUserDetail(c.first);
					}
					else
					{
						toShow.push_back({ c.second.first,percent * 100.0f,c.first });
					}
				}
			}
		}
	}

	if (toShow.size() == 0)
	{
		drawGeneralStats();
		return false;
	}

	float usedData = (float)getData(mTwitterAnalyser->mPanelRetreivedUsers.getUserStructAtIndex(0));
	
	std::sort(toShow.begin(), toShow.end(), [&](const std::tuple<unsigned int, float, u64>& a1, const std::tuple<unsigned int, float, u64>& a2)
		{

			const auto& a1User = mTwitterAnalyser->mInStatsUsers[std::get<2>(a1)];
			const auto& a2User = mTwitterAnalyser->mInStatsUsers[std::get<2>(a2)];

			// apply Jaccard index (https://en.wikipedia.org/wiki/Jaccard_index)
			// a1 subscribers %
			float A1_percent = std::get<1>(a1)*0.01f;
			// a1 intersection size with analysed channel
			float A1_a_inter_b = usedData * A1_percent;
			// a1 union size with analysed channel 
			float A1_a_union_b = usedData + (float)getData(a1User.second) - A1_a_inter_b;

			// a2 subscribers %
			float A2_percent = std::get<1>(a2) * 0.01f;
			// a2 intersection size with analysed channel
			float A2_a_inter_b = usedData * A2_percent;
			// a2 union size with analysed channel 
			float A2_a_union_b = usedData + (float)getData(a2User.second) - A2_a_inter_b;

			float k1 = A1_a_inter_b / A1_a_union_b;
			float k2 = A2_a_inter_b / A2_a_union_b;

			if (k1 == k2)
			{
				return std::get<2>(a1) > std::get<2>(a2);
			}
			return (k1 > k2);

		}
	);

	std::unordered_map<u64, unsigned int>	currentShowedChannels;
	int toShowCount = 0;
	for (auto& tos : toShow)
	{
		currentShowedChannels[std::get<2>(tos)] = 1;
		toShowCount++;

		const auto& a1User = mTwitterAnalyser->mInStatsUsers[std::get<2>(tos)];

		// compute jaccard again
		float fpercent = std::get<1>(tos) * 0.01f;
		float A1_a_inter_b = usedData * fpercent;
		// a1 union size with analysed channel 
		float A1_a_union_b = usedData + (float)getData(a1User.second) - A1_a_inter_b;
		float k1 = A1_a_inter_b / A1_a_union_b;

		std::get<1>(tos) = k1*100.0f;

		if (toShowCount >= (mTwitterAnalyser->mMaxUserCount*3))
			break;
	}

	for (const auto& s : mShowedUser)
	{
		if (currentShowedChannels.find(s.first) == currentShowedChannels.end())
		{
			currentShowedChannels[s.first] = 0;
		}
		else
		{
			currentShowedChannels[s.first]++;
		}
	}

	// add / remove items
	for (const auto& update : currentShowedChannels)
	{
		if (update.second == 0) // to remove 
		{
			auto toremove = mShowedUser.find(update.first);
			mMainInterface->removeItem((*toremove).second);
			mShowedUser.erase(toremove);
			//somethingChanged = true;
		}
		else if (update.second == 1) // to add
		{
			std::string thumbName = "thumbNail_" + TwitterConnect::GetIDString(update.first);
			CMSP toAdd = CoreModifiable::Import("Thumbnail.xml", false, false, nullptr, thumbName);
			toAdd->AddDynamicAttribute<maFloat, float>("Radius", 1.0f);
			mShowedUser[update.first] = toAdd;
			mMainInterface->addItem(toAdd);
			//somethingChanged = true;
		}
	}

	drawSpiral(toShow);
	return false;
}


void CoreFSMStartMethod(GraphDrawer, ReversePercent)
{
	mGoNext = false;
	mCurrentUnit = Reverse_Percent;
	mCurrentStateHasForceDraw = true;
}

void CoreFSMStopMethod(GraphDrawer, ReversePercent)
{
}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(GraphDrawer, ReversePercent))
{
	float wantedpercent = mTwitterAnalyser->mValidUserPercent;

	auto getData = [&](const TwitterConnect::UserStruct& strct)->u32
	{
		if (mTwitterAnalyser->mPanelType == TwitterAnalyser::dataType::Followers)
		{
			return strct.mFollowersCount;
		}
		return strct.mFollowingCount;
	};

	std::vector<std::tuple<unsigned int, float, u64>>	toShow;
	for (auto c : mTwitterAnalyser->mInStatsUsers)
	{
		if (c.first != mTwitterAnalyser->mPanelRetreivedUsers.getUserStructAtIndex(0).mID)
		{
			if (c.second.first > 3)
			{
				float percent = (float)c.second.first / (float)mTwitterAnalyser->mValidUserCount;
				if (percent > wantedpercent)
				{
					const auto& a1User = mTwitterAnalyser->mInStatsUsers[c.first];
					if (a1User.second.mName.ToString() == "") // not loaded
					{
						mTwitterAnalyser->askUserDetail(c.first);
					}
					else
					{
						toShow.push_back({ c.second.first,percent * 100.0f,c.first });
					}
				}
			}
		}
	}

	if (toShow.size() == 0)
	{
		drawGeneralStats();
		return false;
	}

	float usedData = (float)getData(mTwitterAnalyser->mPanelRetreivedUsers.getUserStructAtIndex(0));

	std::sort(toShow.begin(), toShow.end(), [&](const std::tuple<unsigned int, float, u64>& a1, const std::tuple<unsigned int, float, u64>& a2)
		{

			const auto& a1User = mTwitterAnalyser->mInStatsUsers[std::get<2>(a1)];
			const auto& a2User = mTwitterAnalyser->mInStatsUsers[std::get<2>(a2)];

			// apply reverse percent
			// a1 subscribers %
			float A1_percent = std::get<1>(a1) * 0.01f;
			// a1 intersection size with analysed channel
			float A1_a_inter_b = usedData * A1_percent;
			// revpercent
			float A1_revPercent = A1_a_inter_b/(float)getData(a1User.second);
			if (A1_revPercent > 1.0f)
				A1_revPercent = 1.0f;

			// a2 subscribers %
			float A2_percent = std::get<1>(a2) * 0.01f;
			// a2 intersection size with analysed channel
			float A2_a_inter_b = usedData * A2_percent;
			// revpercent
			float A2_revPercent = A2_a_inter_b/(float)getData(a2User.second);
			if (A2_revPercent > 1.0f)
				A2_revPercent = 1.0f;

			if (A1_revPercent == A2_revPercent)
			{
				return std::get<2>(a1) > std::get<2>(a2);
			}
			return (A1_revPercent > A2_revPercent);

		}
	);

	std::unordered_map<u64, unsigned int>	currentShowedChannels;
	int toShowCount = 0;
	for (auto& tos : toShow)
	{
		currentShowedChannels[std::get<2>(tos)] = 1;
		toShowCount++;

		const auto& a1User = mTwitterAnalyser->mInStatsUsers[std::get<2>(tos)];

		// compute rev percent again
		float fpercent = std::get<1>(tos) * 0.01f;
		float A1_a_inter_b = usedData * fpercent;
		// revpercent
		float A1_revPercent = A1_a_inter_b/(float)getData(a1User.second);
		if (A1_revPercent > 1.0f)
			A1_revPercent = 1.0f;

		std::get<1>(tos) = A1_revPercent * 100.0f;

		if (toShowCount >= (mTwitterAnalyser->mMaxUserCount * 3))
			break;
	}

	for (const auto& s : mShowedUser)
	{
		if (currentShowedChannels.find(s.first) == currentShowedChannels.end())
		{
			currentShowedChannels[s.first] = 0;
		}
		else
		{
			currentShowedChannels[s.first]++;
		}
	}

	// add / remove items
	for (const auto& update : currentShowedChannels)
	{
		if (update.second == 0) // to remove 
		{
			auto toremove = mShowedUser.find(update.first);
			mMainInterface->removeItem((*toremove).second);
			mShowedUser.erase(toremove);
			//somethingChanged = true;
		}
		else if (update.second == 1) // to add
		{
			std::string thumbName = "thumbNail_" + TwitterConnect::GetIDString(update.first);
			CMSP toAdd = CoreModifiable::Import("Thumbnail.xml", false, false, nullptr, thumbName);
			toAdd->AddDynamicAttribute<maFloat, float>("Radius", 1.0f);
			mShowedUser[update.first] = toAdd;
			mMainInterface->addItem(toAdd);
			//somethingChanged = true;
		}
	}

	drawSpiral(toShow);
	return false;
}



void CoreFSMStartMethod(GraphDrawer, Normalized)
{
	mGoNext = false;
	mCurrentUnit = 2;
	mCurrentStateHasForceDraw = true;
}

void CoreFSMStopMethod(GraphDrawer, Normalized)
{
}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(GraphDrawer, Normalized))
{
	float wantedpercent = mTwitterAnalyser->mValidUserPercent;

	std::vector<std::tuple<unsigned int, float, u64>>	toShow;
	for (auto c : mTwitterAnalyser->mInStatsUsers)
	{
		if (c.first != mTwitterAnalyser->mPanelRetreivedUsers.getUserStructAtIndex(0).mID)
		{
			if (c.second.first > 3)
			{
				float percent = (float)c.second.first / (float)mTwitterAnalyser->mValidUserCount;
				if (percent > wantedpercent)
				{
					const auto& a1User = mTwitterAnalyser->mInStatsUsers[c.first];
					if (a1User.second.mFollowersCount ==0) // not loaded
					{
						mTwitterAnalyser->askUserDetail(c.first);
					}
					else
					{
						toShow.push_back({ c.second.first,percent * 100.0f,c.first });
					}
				}
			}
		}
	}

	if (toShow.size() == 0)
	{
		drawGeneralStats();
		return false;
	}
	std::sort(toShow.begin(), toShow.end(), [&](const std::tuple<unsigned int, float, u64>& a1, const std::tuple<unsigned int, float, u64>& a2)
		{
			auto& a1User = mTwitterAnalyser->mInStatsUsers[std::get<2>(a1)];
			auto& a2User = mTwitterAnalyser->mInStatsUsers[std::get<2>(a2)];

			float a1fcount = (a1User.second.mFollowersCount < 10) ? logf(10.0f) : logf((float)a1User.second.mFollowersCount);
			float a2fcount = (a2User.second.mFollowersCount < 10) ? logf(10.0f) : logf((float)a2User.second.mFollowersCount);

			float A1_w = ((float)std::get<0>(a1) / a1fcount);
			float A2_w = ((float)std::get<0>(a2) / a2fcount);
			if (A1_w == A2_w)
			{
				return std::get<2>(a1) > std::get<2>(a2);
			}
			return (A1_w > A2_w);
		}
	);

	// once sorted, take the first one
	float NormalizeFollowersCountForShown = 1.0f;

	for (const auto& toS : toShow)
	{
		if (std::get<2>(toS) == mTwitterAnalyser->mPanelRetreivedUsers.getUserStructAtIndex(0).mID)
		{
			continue;
		}

		auto& toPlace = mTwitterAnalyser->mInStatsUsers[std::get<2>(toS)];
		float toplacefcount = (toPlace.second.mFollowersCount < 10) ? logf(10.0f) : logf((float)toPlace.second.mFollowersCount);
		NormalizeFollowersCountForShown = toplacefcount;
		break;
	}

	std::unordered_map<u64, unsigned int>	currentShowedChannels;
	int toShowCount = 0;
	for (auto& tos : toShow)
	{
		currentShowedChannels[std::get<2>(tos)] = 1;
		toShowCount++;

		// compute normalized percent
		float fpercent = std::get<1>(tos);
		int followcount = mTwitterAnalyser->mInStatsUsers[std::get<2>(tos)].second.mFollowersCount;
		float toplacefcount = (followcount < 10) ? logf(10.0f) : logf((float)followcount);
		fpercent *= NormalizeFollowersCountForShown / toplacefcount;

		std::get<1>(tos) = fpercent;

		if (toShowCount >= (mTwitterAnalyser->mMaxUserCount*3))
			break;
	}

	for (const auto& s : mShowedUser)
	{
		if (currentShowedChannels.find(s.first) == currentShowedChannels.end())
		{
			currentShowedChannels[s.first] = 0;
		}
		else
		{
			currentShowedChannels[s.first]++;
		}
	}

	// add / remove items
	for (const auto& update : currentShowedChannels)
	{
		if (update.second == 0) // to remove
		{
			auto toremove = mShowedUser.find(update.first);
			mMainInterface->removeItem((*toremove).second);
			mShowedUser.erase(toremove);
			//somethingChanged = true;
		}
		else if (update.second == 1) // to add
		{
			std::string thumbName = "thumbNail_" + TwitterConnect::GetIDString(update.first);
			CMSP toAdd = CoreModifiable::Import("Thumbnail.xml", false, false, nullptr, thumbName);
			toAdd->AddDynamicAttribute<maFloat, float>("Radius", 1.0f);
			mShowedUser[update.first] = toAdd;
			mMainInterface->addItem(toAdd);
			//somethingChanged = true;
		}
	}

	drawSpiral(toShow);
	return false;
}

void CoreFSMStartMethod(GraphDrawer, Force)
{
	mForcedBaseStartingTime = KigsCore::GetCoreApplication()->GetApplicationTimer()->GetTime();
	prepareForceGraphData();
	mTwitterAnalyser->ChangeAutoUpdateFrequency(this, 50.0);
	mCurrentStateHasForceDraw = true;
}

void CoreFSMStopMethod(GraphDrawer, Force)
{
	mTwitterAnalyser->ChangeAutoUpdateFrequency(this, 1.0);
}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(GraphDrawer, Force))
{
	drawForce();
	return false;
}

void CoreFSMStartMethod(GraphDrawer, UserStats)
{
	mGoNext = false;
	mCurrentStateHasForceDraw = false;
	for (auto& u : mShowedUser)
	{
		const CMSP& toSetup = u.second;
		toSetup("IsHidden") = true;
	}

	// move logo
	mMainInterface["thumbnail"]("Dock") = v2f(0.94f, 0.08f);

	CMSP uiImage = KigsCore::GetInstanceOf("drawuiImage", "UIImage");
	uiImage->setValue("Priority", 10);
	uiImage->setValue("Anchor", v2f(0.0f, 0.0f));
	uiImage->setValue("Dock", v2f(0.0f, 0.0f));
	uiImage->setValue("Color", v3f(1.0f, 1.0f, 1.0f));
	CMSP drawtexture = KigsCore::GetInstanceOf("drawTexture", "Texture");
	drawtexture->setValue("FileName", "");

	GetUpgrador()->mBitmap = KigsCore::GetInstanceOf("mBitmap", "KigsBitmap");
	GetUpgrador()->mBitmap->setValue("Size", v2f(2048, 2048));
	drawtexture->addItem(GetUpgrador()->mBitmap);

	mMainInterface->addItem(uiImage);
	uiImage->Init();

	drawtexture->Init();
	GetUpgrador()->mBitmap->Init();
	uiImage->addItem(drawtexture);

}

void CoreFSMStopMethod(GraphDrawer, UserStats)
{
	for (auto& u : mShowedUser)
	{
		const CMSP& toSetup = u.second;
		toSetup("IsHidden") = false;
	}
	GetUpgrador()->mBitmap = nullptr;

	CMSP uiImage = mMainInterface->GetFirstSonByName("UIImage","drawuiImage");
	mMainInterface->removeItem(uiImage);
	// move logo
	mMainInterface["thumbnail"]("Dock") = v2f(0.52f, 0.44f);
}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(GraphDrawer, UserStats))
{
	drawStats(GetUpgrador()->mBitmap);
	return false;
}

void CoreFSMStartMethod(GraphDrawer, TopDraw)
{
	mGoNext = false;
	mCurrentUnit = 0;

	// just show the count, not a %
	mCurrentUnit = AnonymousCount;
	mShowMyself = true;
	
	mCurrentStateHasForceDraw = false;
}

void CoreFSMStopMethod(GraphDrawer, TopDraw)
{
}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(GraphDrawer, TopDraw))
{
	if (mTwitterAnalyser->mInStatsUsers.size() == 0)
	{
		drawGeneralStats();
		return false;
	}

	std::vector<std::pair<u32, u64>>	sortTop;
	for (const auto& c : mTwitterAnalyser->mInStatsUsers)
	{
		sortTop.push_back({ c.second.first,c.first });
	}

	std::sort(sortTop.begin(), sortTop.end(), [&](const std::pair<u32, u64>& a1, const std::pair<u32, u64>& a2)
		{
			if (a1.first == a2.first)
			{
				// search followers count
				if (mTwitterAnalyser->mPanelRetreivedUsers.getUserStruct(a1.second).mFollowersCount == mTwitterAnalyser->mPanelRetreivedUsers.getUserStruct(a2.second).mFollowersCount)
				{
					return a1.second > a2.second;
				}

				return mTwitterAnalyser->mPanelRetreivedUsers.getUserStruct(a1.second).mFollowersCount > mTwitterAnalyser->mPanelRetreivedUsers.getUserStruct(a2.second).mFollowersCount;
				
			}
			return a1.first > a2.first;
		}
	);

	if (sortTop.size() > (mTwitterAnalyser->mMaxUserCount*3))
	{
		sortTop.resize(mTwitterAnalyser->mMaxUserCount*3);
	}
	
	float totalCount = 0.0f;
	u32 maxCount = 0;
	for (const auto& c : sortTop)
	{
		totalCount += (float)c.first;
		if (maxCount < c.first)
			maxCount = c.first;
	}
	float oneOnCount = 1.0f / maxCount;

	bool usesqrscale = false;
	if ((mTwitterAnalyser->mPanelType == TwitterAnalyser::dataType::Followers) ||
		(mTwitterAnalyser->mPanelType == TwitterAnalyser::dataType::Following))
	{
		usesqrscale = true;
		oneOnCount = 1.0f / (sqrtf(10.0f + (float)maxCount));
	}
	
	std::vector<std::tuple<unsigned int, float, u64>>	toShow;

	for (const auto& c : sortTop)
	{
		float percent = (float)c.first * oneOnCount * 100.0f;
		if (usesqrscale)
		{
			percent = sqrtf(10.0f+(float)c.first)* oneOnCount * 100.0f;
		}
		toShow.push_back({ c.first,percent,c.second });
	}

	std::unordered_map<u64, unsigned int>	currentShowedChannels;
	int toShowCount = 0;
	for (const auto& tos : toShow)
	{
		currentShowedChannels[std::get<2>(tos)] = 1;
		toShowCount++;

		const auto& a1User = mTwitterAnalyser->mInStatsUsers[std::get<2>(tos)];

		if (toShowCount >= (mTwitterAnalyser->mMaxUserCount*3))
			break;
	}

	for (const auto& s : mShowedUser)
	{
		if (currentShowedChannels.find(s.first) == currentShowedChannels.end())
		{
			currentShowedChannels[s.first] = 0;
		}
		else
		{
			currentShowedChannels[s.first]++;
		}
	}

	// add / remove items
	for (const auto& update : currentShowedChannels)
	{
		if (update.second == 0) // to remove 
		{
			auto toremove = mShowedUser.find(update.first);
			mMainInterface->removeItem((*toremove).second);
			mShowedUser.erase(toremove);
			//somethingChanged = true;
		}
		else if (update.second == 1) // to add
		{
			std::string thumbName = "thumbNail_" + TwitterConnect::GetIDString(update.first);
			CMSP toAdd = CoreModifiable::Import("Thumbnail.xml", false, false, nullptr, thumbName);
			toAdd->AddDynamicAttribute<maFloat, float>("Radius", 1.0f);
			mShowedUser[update.first] = toAdd;
			mMainInterface->addItem(toAdd);
			//somethingChanged = true;
		}
	}

	drawSpiral(toShow);
	return false;
}

void	GraphDrawer::OpeningClosingUpdate(bool closing)
{
	// retrieve analysed account following count

	auto fcount=mTwitterAnalyser->mPanelRetreivedUsers.getUserStructAtIndex(0).mFollowingCount;

	float gcoef = log10f((float)fcount) / log10f(100);

	std::vector<std::pair<u64, float>>	sortOpeningClosing;
	for (const auto& c : mTwitterAnalyser->mPerPanelUsersStats)
	{
		size_t ulistSize = c.second.size();
		if (ulistSize>5)
		{
			float total_w = 0.0f;
			float follow_w = 0.0f;
			for (const auto& u : c.second.getList())
			{
				total_w += u.second;
				// check if u is in panel (following)
				if (mTwitterAnalyser->mPanelUserList.hasUser(u.first))
				{
					follow_w += u.second;
				}
			}

			float score = gcoef*(follow_w / total_w);
			if (score > 1.0f)
				score = 1.0f;
			if (!closing)
			{
				score = 1.0f - score;
			}

			if (ulistSize < 100)
				score *= ((float)ulistSize) * 0.01f;

			sortOpeningClosing.push_back({ c.first, score });
		}
	}

	std::sort(sortOpeningClosing.begin(), sortOpeningClosing.end(), [&](const std::pair<u64, float>& a1, const std::pair<u64, float>& a2)
		{
			if (a1.second == a2.second)
			{
				return a1.first > a2.first;
			}
			return a1.second > a2.second;
		}
	);

	if (sortOpeningClosing.size() > (mTwitterAnalyser->mMaxUserCount * 3))
	{
		sortOpeningClosing.resize(mTwitterAnalyser->mMaxUserCount * 3);
	}

	std::vector<std::tuple<unsigned int, float, u64>>	toShow;

	for (const auto& c : sortOpeningClosing)
	{
		float percent = (float)c.second * 100.0f;
		toShow.push_back({ percent,percent,c.first });
	}

	std::unordered_map<u64, unsigned int>	currentShowedChannels;
	int toShowCount = 0;
	for (const auto& tos : toShow)
	{
		currentShowedChannels[std::get<2>(tos)] = 1;
		toShowCount++;

		if (toShowCount >= (mTwitterAnalyser->mMaxUserCount * 3))
			break;
	}

	for (const auto& s : mShowedUser)
	{
		if (currentShowedChannels.find(s.first) == currentShowedChannels.end())
		{
			currentShowedChannels[s.first] = 0;
		}
		else
		{
			currentShowedChannels[s.first]++;
		}
	}

	// add / remove items
	for (const auto& update : currentShowedChannels)
	{
		if (update.second == 0) // to remove 
		{
			auto toremove = mShowedUser.find(update.first);
			mMainInterface->removeItem((*toremove).second);
			mShowedUser.erase(toremove);
		}
		else if (update.second == 1) // to add
		{
			std::string thumbName = "thumbNail_" + TwitterConnect::GetIDString(update.first);
			CMSP toAdd = CoreModifiable::Import("Thumbnail.xml", false, false, nullptr, thumbName);
			toAdd->AddDynamicAttribute<maFloat, float>("Radius", 1.0f);
			mShowedUser[update.first] = toAdd;
			mMainInterface->addItem(toAdd);
		}
	}

	drawSpiral(toShow);
}

// Opening
void CoreFSMStartMethod(GraphDrawer, OpeningMeasurement)
{
	mGoNext = false;
	// just show the count, not a %
	mCurrentUnit = Opening;
	mShowMyself = false;

	mCurrentStateHasForceDraw = false;
}

void CoreFSMStopMethod(GraphDrawer, OpeningMeasurement)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(GraphDrawer, OpeningMeasurement))
{
	if (mTwitterAnalyser->mInStatsUsers.size() == 0)
	{
		drawGeneralStats();
		return false;
	}

	OpeningClosingUpdate(false);

	return false;
}

// Closing
void CoreFSMStartMethod(GraphDrawer, ClosingMeasurement)
{
	mGoNext = false;

	// just show the count, not a %
	mCurrentUnit = Closing;
	mShowMyself = false;
	mCurrentStateHasForceDraw = false;
}

void CoreFSMStopMethod(GraphDrawer, ClosingMeasurement)
{
}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(GraphDrawer, ClosingMeasurement))
{
	if (mTwitterAnalyser->mInStatsUsers.size() == 0)
	{
		drawGeneralStats();
		return false;
	}

	OpeningClosingUpdate(true);
	return false;
}

// Closing
void CoreFSMStartMethod(GraphDrawer, OpeningClosingStats)
{
	mGoNext = false;

	// just show the count, not a %
	mCurrentUnit = AnonymousCount;
	mShowMyself = false;
	mCurrentStateHasForceDraw = false;

	for (auto& u : mShowedUser)
	{
		const CMSP& toSetup = u.second;
		toSetup("IsHidden") = true;
	}

	// move logo
	mMainInterface["thumbnail"]("Dock") = v2f(0.94f, 0.08f);

	CMSP uiImage = KigsCore::GetInstanceOf("drawuiImage", "UIImage");
	uiImage->setValue("Priority", 10);
	uiImage->setValue("Anchor", v2f(0.0f, 0.0f));
	uiImage->setValue("Dock", v2f(0.0f, 0.0f));
	uiImage->setValue("Color", v3f(1.0f, 1.0f, 1.0f));
	CMSP drawtexture = KigsCore::GetInstanceOf("drawTexture", "Texture");
	drawtexture->setValue("FileName", "");

	GetUpgrador()->mBitmap = KigsCore::GetInstanceOf("mBitmap", "KigsBitmap");
	GetUpgrador()->mBitmap->setValue("Size", v2f(2048, 2048));
	drawtexture->addItem(GetUpgrador()->mBitmap);

	mMainInterface->addItem(uiImage);
	uiImage->Init();

	drawtexture->Init();
	GetUpgrador()->mBitmap->Init();
	uiImage->addItem(drawtexture);

}

void CoreFSMStopMethod(GraphDrawer, OpeningClosingStats)
{
	for (auto& u : mShowedUser)
	{
		const CMSP& toSetup = u.second;
		toSetup("IsHidden") = false;
	}
	GetUpgrador()->mBitmap = nullptr;

	CMSP uiImage = mMainInterface->GetFirstSonByName("UIImage", "drawuiImage");
	mMainInterface->removeItem(uiImage);
	// move logo
	mMainInterface["thumbnail"]("Dock") = v2f(0.52f, 0.44f);
}

void	GraphDrawer::drawOCStats(SP<KigsBitmap> bitmap)
{
	drawGeneralStats();
	bitmap->Clear({ 0,0,0,0 });

	// title
	bitmap->Print("Opening / Closing statistics", 1920 / 2, 16, 1, 1920, 92, "Calibri.ttf", 1, { 0,0,0,128 });


	auto fcount = mTwitterAnalyser->mPanelRetreivedUsers.getUserStructAtIndex(0).mFollowingCount;

	float gcoef = log10f((float)fcount)/log10f(100);

	std::vector<float>	Opening;
	std::vector<float>	Closing;
	for (const auto& c : mTwitterAnalyser->mPerPanelUsersStats)
	{
		size_t ulistSize = c.second.size();
		if (ulistSize > 5)
		{
			float total_w = 0.0f;
			float follow_w = 0.0f;
			for (const auto& u : c.second.getList())
			{
				total_w += u.second;
				// check if u is in panel (following)
				if (mTwitterAnalyser->mPanelUserList.hasUser(u.first))
				{
					follow_w += u.second;
				}
			}

			float cscore = gcoef * (follow_w / total_w);
			if (cscore > 1.0f)
				cscore = 1.0f;
			
			float oscore= 1.0f - cscore;
		
			if (ulistSize < 100)
			{
				oscore *= ((float)ulistSize) * 0.01f;
				cscore *= ((float)ulistSize) * 0.01f;
			}

			Opening.push_back(oscore*100.0f);
			Closing.push_back(cscore*100.0f);
		}
	}

	Diagram	diagram(bitmap);
	diagram.mZonePos = v2i(64, 256);
	diagram.mColumnCount = 20;
	diagram.mZoneSize = v2i(856, 512);
	diagram.mLimits = v2f(0.0f, 100.0f);
	diagram.mUseLog = false;
	diagram.mTitle = "Opening Profile";
	diagram.mColumnColor = { 94,169,221,255 };
	if (Opening.size())
	{
		diagram.Draw(Opening);
	}

	diagram.mZonePos = v2i(1920 - (856+64), 256);
	diagram.mColumnCount = 20;
	diagram.mZoneSize = v2i(856, 512);
	diagram.mLimits = v2f(0.0f, 100.0f);
	diagram.mUseLog = false;
	diagram.mTitle = "Closing Profile";
	diagram.mColumnColor = { 94,169,221,255 };
	if (Closing.size())
	{
		diagram.Draw(Closing);
	}
}


DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(GraphDrawer, OpeningClosingStats))
{
	drawOCStats(GetUpgrador()->mBitmap);
	return false;
}

