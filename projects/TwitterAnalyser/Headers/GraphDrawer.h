#pragma once

#include "CoreModifiable.h"
#include "CoreBaseApplication.h"
#include "CoreFSMState.h"
#include "TwitterConnect.h"
#include "KigsBitmap.h"

namespace Kigs
{
	using namespace Kigs::Core;
	using namespace Kigs::Fsm;

	class TwitterAnalyser;

	// manage drawing of current graph
	class GraphDrawer : public CoreModifiable
	{
	protected:
		CMSP				mMainInterface;

		TwitterAnalyser* mTwitterAnalyser = nullptr;
		void	drawSpiral(std::vector<std::tuple<unsigned int, float, u64>>& toShow);
		void	drawForce();
		void	drawStats(SP<Draw::KigsBitmap> bitmap);
		void	drawOCStats(SP<Draw::KigsBitmap> bitmap);
		void	drawGeneralStats();

		bool	mEverythingDone = false;

		std::unordered_map<u64, CMSP>									mShowedUser;

		enum Measure
		{
			Percent = 0,
			Similarity = 1,
			Normalized = 2,
			AnonymousCount = 3,
			Opening = 4,
			Closing = 5,
			Reverse_Percent = 6,
			MEASURE_COUNT = 7
		};

		const std::string	mUnits[MEASURE_COUNT] = { " \%"," sc"," n",""," Op"," Cl", " r%" };

		u32		mCurrentUnit = 0;

		double mForcedBaseStartingTime = 0.0;

		void	prepareForceGraphData();

		std::unordered_map<u64, TwitterConnect::PerAccountUserMap>	mAccountSubscriberMap;

		maBool	mDrawForce = BASE_ATTRIBUTE(DrawForce, false);
		bool	mCurrentStateHasForceDraw = false;
		bool	mShowMyself = false;

		void	OpeningClosingUpdate(bool closing);

		class Diagram
		{
			v2i				mZonePos = { 128,128 };
			v2i				mZoneSize = { 384,256 };
			v2f				mLimits = { 0.0f, -1.0f };
			std::string		mTitle = "Diagram";
			SP<Draw::KigsBitmap>	mBitmap;
			u32				mColumnCount = 12;
			bool			mUseLog;
			// apply multiplier, then shift
			float			mMultiplier = 0.0f;
			float			mShift = 0.0f;

			Draw::KigsBitmap::KigsBitmapPixel	mColumnColor = { 0,0,0,255 };
			friend class GraphDrawer;

		public:
			Diagram(SP<Draw::KigsBitmap> bitmap) : mBitmap(bitmap)
			{

			}

			template<typename T>
			void	Draw(const std::vector<T>& values);
		};

	public:
		DECLARE_CLASS_INFO(GraphDrawer, CoreModifiable, GraphDrawer);
		DECLARE_CONSTRUCTOR(GraphDrawer);

		void	setInterface(CMSP minterface)
		{
			mMainInterface = minterface;
		}

		void InitModifiable() override;

		maBool  mGoNext = BASE_ATTRIBUTE(GoNext, false);
		void	setEverythingDone()
		{
			mEverythingDone = true;
		}

		void	nextDrawType();

	};

	// Percent drawing
	START_DECLARE_COREFSMSTATE(GraphDrawer, Percent)
	COREFSMSTATE_WITHOUT_METHODS()
	END_DECLARE_COREFSMSTATE()

	START_DECLARE_COREFSMSTATE(GraphDrawer, TopDraw)
	COREFSMSTATE_WITHOUT_METHODS()
	END_DECLARE_COREFSMSTATE()

	// Normalized drawing
	START_DECLARE_COREFSMSTATE(GraphDrawer, Normalized)
	COREFSMSTATE_WITHOUT_METHODS()
	END_DECLARE_COREFSMSTATE()

	// Opening drawing
	START_DECLARE_COREFSMSTATE(GraphDrawer, OpeningMeasurement)
	COREFSMSTATE_WITHOUT_METHODS()
	END_DECLARE_COREFSMSTATE()

	// Closing drawing
	START_DECLARE_COREFSMSTATE(GraphDrawer, ClosingMeasurement)
	COREFSMSTATE_WITHOUT_METHODS()
	END_DECLARE_COREFSMSTATE()

	// Opening/Closing stats
	START_DECLARE_COREFSMSTATE(GraphDrawer, OpeningClosingStats)
	CMSP	mBitmap;
	COREFSMSTATE_WITHOUT_METHODS()
	END_DECLARE_COREFSMSTATE()

	// Jaccard Drawing
	START_DECLARE_COREFSMSTATE(GraphDrawer, Jaccard)
	COREFSMSTATE_WITHOUT_METHODS()
	END_DECLARE_COREFSMSTATE()

	// Reverse % Drawing (only on followers/following)
	START_DECLARE_COREFSMSTATE(GraphDrawer, ReversePercent)
	COREFSMSTATE_WITHOUT_METHODS()
	END_DECLARE_COREFSMSTATE()

	// user stats drawing
	START_DECLARE_COREFSMSTATE(GraphDrawer, UserStats)
	CMSP	mBitmap;
	COREFSMSTATE_WITHOUT_METHODS()
	END_DECLARE_COREFSMSTATE()

	// Force drawing
	START_DECLARE_COREFSMSTATE(GraphDrawer, Force)
	COREFSMSTATE_WITHOUT_METHODS()
	END_DECLARE_COREFSMSTATE()

}