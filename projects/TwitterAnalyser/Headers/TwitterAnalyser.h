#pragma once

#include <DataDrivenBaseApplication.h>
#include "Texture.h"
#include "HTTPConnect.h"

class TwitterAnalyser : public DataDrivenBaseApplication
{
public:
	DECLARE_CLASS_INFO(TwitterAnalyser, DataDrivenBaseApplication, Core);
	DECLARE_CONSTRUCTOR(TwitterAnalyser);

protected:

	SP<HTTPConnect>									mTwitterConnect = nullptr;
	SP<HTTPAsyncRequest>							mAnswer = nullptr;

	void	ProtectedInit() override;
	void	ProtectedUpdate() override;
	void	ProtectedClose() override;


	DECLARE_METHOD(getFollowers);
	DECLARE_METHOD(getFollowing);
	DECLARE_METHOD(getUserDetails);
	COREMODIFIABLE_METHODS(getFollowers, getFollowing, getUserDetails);

	void	thumbnailReceived(CoreRawBuffer* data, CoreModifiable* downloader);
	void	switchDisplay();
	WRAP_METHODS(thumbnailReceived, switchDisplay);

	CoreItemSP	RetrieveJSON(CoreModifiable* sender);

	
	void	ProtectedInitSequence(const kstl::string& sequence) override;
	void	ProtectedCloseSequence(const kstl::string& sequence) override;
	CoreItemSP	LoadJSon(const std::string& fname, bool utf16 = false);
	void		SaveJSon(const std::string& fname,const CoreItemSP& json, bool utf16 = false);

	std::vector<std::string>	mTwitterBear;
	unsigned int				mCurrentBearer = 0;
	unsigned int				NextBearer()
	{
		mCurrentBearer = (mCurrentBearer + (unsigned int)1) % mTwitterBear.size();
		return mCurrentBearer;
	}
	unsigned int				CurrentBearer()
	{
		return mCurrentBearer;
	}
	std::string mUserName;

	unsigned int mUserPanelSize;
	unsigned int mWantedTotalPanelSize=100000;
	unsigned int mMaxUserCount;
	float		 mValidUserPercent;

	// current application state 
	unsigned int					mState = 0;

	class ThumbnailStruct
	{
	public:
		SP<Texture>					mTexture = nullptr;
		std::string					mURL="";
	};

	class UserStruct
	{
	public:
		usString					mName="";
		unsigned int				mFollowersCount = 0;
		unsigned int				mFollowingCount = 0;
		ThumbnailStruct				mThumb;
	};

	CMSP			mMainInterface;
	u64				mUserID;
	UserStruct		mCurrentUser;

	std::vector<std::pair<CMSP, std::pair<u64, UserStruct*>> >		mDownloaderList;
	std::map<u64, std::pair<unsigned int, UserStruct>>				mFollowersFollowingCount;

	std::unordered_map<u64, CMSP>			mShowedUser;

	// list of followers
	std::vector<u64>												mFollowers;
	unsigned int													mTreatedFollowerIndex = 0;
	unsigned int													mTreatedFollowerCount = 0;
	unsigned int													mCurrentStartingFollowerList = 0;
	void	NextTreatedFollower()
	{
		mTreatedFollowerIndex += mFollowers.size() / 7;
		if (mTreatedFollowerIndex >= mFollowers.size())
		{
			mCurrentStartingFollowerList++;
			mTreatedFollowerIndex = mCurrentStartingFollowerList;
		}
		mTreatedFollowerCount++;
	}


	// manage following for one user
	std::vector<u64>												mCurrentFollowing;

	void		LaunchDownloader(u64 id, UserStruct& ch);
	bool		LoadUserStruct(u64 id, UserStruct& ch, bool requestThumb);
	void		SaveUserStruct(u64 id, UserStruct& ch);

	bool		LoadFollowersFile();
	void		SaveFollowersFile();

	bool		LoadFollowingFile(u64 id);
	void		SaveFollowingFile(u64 id);

	void		UpdateStatistics();

	std::vector<u64>		LoadIDVectorFile(const std::string& filename);
	void					SaveIDVectorFile(const std::vector<u64>& v, const std::string& filename);

	bool		LoadThumbnail(u64 id, UserStruct& ch);
	void		refreshAllThumbs();

	static std::string	GetUserFolderFromID(u64 id);
	static std::string	GetIDString(u64 id);
	static std::string  CleanURL(const std::string& url);

	// get current time once at launch to compare with modified file date
	time_t     mCurrentTime = 0;
	// if a file is older ( in seconds ) than this limit, then it's considered as not existing ( to recreate )
	double		mOldFileLimit = 0.0;
	double		mLastUpdate;
	bool		mShowInfluence = false;
	bool		mWaitQuota = false;
	unsigned int	mWaitQuotaCount = 0;

	std::string	mFollowersNextCursor = "-1";

	unsigned int									myRequestCount = 0;
	double		mStartWaitQuota=0.0;

	// manage wait time between requests

	void	RequestLaunched(double toWait)
	{
		mNextRequestDelay = toWait/ mTwitterBear.size();
		mLastRequestTime = GetApplicationTimer()->GetTime();
	}

	bool	CanLaunchRequest()
	{
		double dt = GetApplicationTimer()->GetTime() - mLastRequestTime;
		if (dt > mNextRequestDelay)
		{
			return true;
		}
		return false;
	}

	double		mNextRequestDelay = 0.0;
	double		mLastRequestTime = 0.0;

	std::vector<u64>			mUserDetailsAsked;
};
