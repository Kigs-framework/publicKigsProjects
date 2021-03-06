#include "YoutubeAnalyser.h"
#include "FilePathManager.h"
#include "NotificationCenter.h"
#include "HTTPRequestModule.h"
#include "HTTPConnect.h"
#include "JSonFileParser.h"
#include "ResourceDownloader.h"
#include "TinyImage.h"
#include "JPEGClass.h"
#include "PNGClass.h"
#include "UI/UIImage.h"
#include "TextureFileManager.h"
#include "Histogram.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
#include <unistd.h>
#endif

//#define LOG_ALL
#ifdef LOG_ALL
SmartPointer<::FileHandle> LOG_File = nullptr;

void	log(std::string logline)
{
	if (LOG_File->myFile)
	{
		logline += "\n";
		Platform_fwrite(logline.c_str(), 1, logline.length(), LOG_File.get());
	}
}

void closeLog()
{
	Platform_fclose(LOG_File.get());
}

#endif

IMPLEMENT_CLASS_INFO(YoutubeAnalyser);

IMPLEMENT_CONSTRUCTOR(YoutubeAnalyser)
{

}

void	replaceSpacesByEscapedOr(std::string& keyw)
{
	size_t pos = keyw.find(" ");
	while (pos != std::string::npos)
	{
		keyw.replace(pos, 1, "%7C");
		pos = keyw.find(" ");
	}
}

void	YoutubeAnalyser::ProtectedInit()
{
	// Base modules have been created at this point

	// lets say that the update will sleep 1ms
	SetUpdateSleepTime(1);

	mCurrentTime = time(0);

	// TODO : get this in parameter file
	// here don't use files olders than three months.
	mOldFileLimit = 60.0 * 60.0 * 24.0 * 30.0 * 3.0;

	SP<FilePathManager>& pathManager = KigsCore::Singleton<FilePathManager>();
	// when a path is given, search the file only with this path
	pathManager->setValue("StrictPath", true);

	pathManager->AddToPath(".", "json");

	// Init App
	JSonFileParser L_JsonParser;
	CoreItemSP initP = L_JsonParser.Get_JsonDictionary("launchParams.json");

	// retreive parameters
	mKey = "&key=" + (std::string)initP["GoogleKey"];
	mChannelName = initP["ChannelID"];

	auto SetMemberFromParam = [&](auto& x, const char* id) {
		if (!initP[id].isNil()) x = initP[id];
	};

	SetMemberFromParam(mSubscribedUserCount, "ValidUserCount");
	SetMemberFromParam(mMaxChannelCount, "MaxChannelCount");
	SetMemberFromParam(mValidChannelPercent, "ValidChannelPercent");
	SetMemberFromParam(mMaxUserPerVideo, "MaxUserPerVideo");
	SetMemberFromParam(mUseKeyword, "UseKeyword");

	replaceSpacesByEscapedOr(mUseKeyword);

	int oldFileLimitInDays=3*30;
	SetMemberFromParam(oldFileLimitInDays, "OldFileLimitInDays");
	
	mOldFileLimit = 60.0 * 60.0 * 24.0 * (double)oldFileLimitInDays;

	CoreCreateModule(HTTPRequestModule, 0);

	
	// init google api connection
	mGoogleConnect = KigsCore::GetInstanceOf("googleConnect", "HTTPConnect");
	mGoogleConnect->setValue("HostName", "www.googleapis.com");
	mGoogleConnect->setValue("Type", "HTTPS");
	mGoogleConnect->setValue("Port", "443");
	mGoogleConnect->Init();
	
	// search if current channel is in Cached data

	std::string currentChannelProgress = "Cache/" + mChannelName + "/";
	currentChannelProgress += mChannelName + ".json";
	CoreItemSP currentP = LoadJSon(currentChannelProgress);

	if (currentP.isNil()) // new channel, launch getChannelID
	{
		std::string url = "/youtube/v3/channels?part=snippet,statistics&id=";
		std::string request = url + mChannelName + mKey;
		mAnswer = mGoogleConnect->retreiveGetAsyncRequest(request.c_str(), "getChannelID", this);
		printf("Request : getChannelID\n ");
		mAnswer->Init();
		myRequestCount++;
	}
	else // load current channel
	{
		mChannelID = currentP["channelID"];
		if (!LoadChannelStruct(mChannelID, mChannelInfos, true))
		{
			mFoundChannels[mChannelID] = &mChannelInfos;
			// request details
			std::string url = "/youtube/v3/channels?part=statistics,snippet&id=";
			std::string request = url + mChannelID + mKey;
			mAnswer = mGoogleConnect->retreiveGetAsyncRequest(request.c_str(), "getChannelStats", this);
			printf("Request : getChannelStats %s \n ", mChannelID.c_str());
			mAnswer->AddDynamicAttribute(CoreModifiable::ATTRIBUTE_TYPE::INT,"dontSetState", 1);
			mAnswer->AddDynamicAttribute(CoreModifiable::ATTRIBUTE_TYPE::STRING, "ID", mChannelID.c_str());
			mAnswer->Init();
			myRequestCount++;
		}
		else
		{
			mState = 1;
		}
	}

#ifdef LOG_ALL
	LOG_File = Platform_fopen("Log_All.txt", "wb");
#endif

	// Load AppInit, GlobalConfig then launch first sequence
	DataDrivenBaseApplication::ProtectedInit();
	mLastUpdate = GetApplicationTimer()->GetTime();
}

void	YoutubeAnalyser::ProtectedUpdate()
{
	DataDrivenBaseApplication::ProtectedUpdate();
	
	if (mDrawForceBased)
	{
		DrawForceBased();
		return;
	}

	// everything is done
	if (mState == 50)
	{
		// just refresh graphics
		double dt = GetApplicationTimer()->GetTime() - mLastUpdate;
		if (dt > 1.0)
		{
			mLastUpdate = GetApplicationTimer()->GetTime();
			refreshAllThumbs();
		}
		return;
	}

	bool needMoreData = false;

	// goal was reached ?
	if (mySubscribedWriters >= mSubscribedUserCount)
	{
		if(mState != 50) // finished ?
			mState = 0; // stop updating
	}

	// check state
	switch(mState)
	{
	case 60: // error sequence
	case 0: // wait
		break;
	case 11: // retrieve more videos for this channel
#ifdef LOG_ALL
		log("********************    state 11");
#endif
		needMoreData = true;
	case 1: // process this channel
	{
#ifdef LOG_ALL
		log("********************    state 1");
#endif
		mState = 0; // will wait 
		mNotSubscribedUserForThisVideo = 0;
		mBadVideo = false;
		// search video list in cache

		std::string filename = "Cache/" + mChannelName + "/videos/";
		filename += mChannelID + ".json";

		CoreItemSP initP = LoadJSon(filename);
		// need to get more videos than the ones already retrieved ?
		std::string nextPageToken;
		if (needMoreData && (!initP.isNil()))
		{
			if (!initP["nextPageToken"].isNil())
			{
				nextPageToken = "&pageToken=" + (std::string)initP["nextPageToken"];
			}
		}

		if (initP.isNil() || (nextPageToken!=""))
		{
			// get latest video list
			// GET https://www.googleapis.com/youtube/v3/search?part=snippet&channelId={CHANNEL_ID}&maxResults=10&order=date&type=video&key={YOUR_API_KEY}
			
			// https://www.googleapis.com/youtube/v3/search?part=snippet&maxResults=25&q=surfing&key={YOUR_API_KEY}
			
			std::string url = "/youtube/v3/search?part=snippet";

			if (mUseKeyword == "")
			{
				url += "&channelId=" + mChannelID;
			}
			else
			{
				url += "&regionCode=FR&relevanceLanguage=fr&q=" + mUseKeyword;
			}
			std::string request = url  + mKey + nextPageToken + "&maxResults=50&order=date&type=video";
			
			// TODO : add date parameter
			//"&publishedAfter=""
			//"&publishedBefore=""
			mAnswer = mGoogleConnect->retreiveGetAsyncRequest(request.c_str(), "getVideoList", this);
			printf("Request : getVideoList\n ");
			mAnswer->Init();
			myRequestCount++;
		}
		else // get video list from file
		{
			CoreItemSP videoList = initP["videos"];
			CoreItemSP videotitles = initP["videosTitles"];
			if (!videoList.isNil())
			{
				mVideoListToProcess.clear();
				for (int i = 0; i < videoList->size(); i++)
				{
					mVideoListToProcess.push_back({ videoList[i], videotitles[i] });
				}
				mState = 2; // then go to process videos
			}
			else
			{
				mState = 10; // error  
			}
		}
	}
	break;
	case 21: // retrieve more comments from this video
		needMoreData = true;

#ifdef LOG_ALL
		log("********************    state 21");
#endif
	case 2: // treat video list
	{
#ifdef LOG_ALL
		log("********************    state 2");
#endif
		mState = 0; // wait

		if (mVideoListToProcess.size()> mCurrentProcessedVideo)
		{
			mAuthorListToProcess.clear();
			mCurrentAuthorList.clear();
			// https://www.googleapis.com/youtube/v3/commentThreads?part=snippet%2Creplies&videoId=_VB39Jo8mAQ&key=[YOUR_API_KEY]' 
			std::string videoID = mVideoListToProcess[mCurrentProcessedVideo].first;

			// set current video title 
			mMainInterface["CurrentVideo"]("Text") = mVideoListToProcess[mCurrentProcessedVideo].second;

#ifdef LOG_ALL
			log(std::string("Process video ")+ videoID);
#endif
			std::string filename = "Cache/" + mChannelName + "/videos/";
			filename += videoID + "_videos.json";

			CoreItemSP initP = LoadJSon(filename);

			// check if we can retrieve more comments from the same video
			std::string nextPageToken;
			if (needMoreData && (!initP.isNil()))
			{
				if (!initP["nextPageToken"].isNil())
				{
					nextPageToken = "&pageToken=" + (std::string)initP["nextPageToken"];
				}
				else
				{
					mState = 2;
					mCurrentProcessedVideo++;
					mCurrentVideoUserFound = 0;
					mNotSubscribedUserForThisVideo = 0;
					break;
				}
			}			

			// request new comments
			if (initP.isNil() || nextPageToken!="")
			{
				std::string url = "/youtube/v3/commentThreads?part=snippet%2Creplies&videoId=";
				std::string request = url + videoID + mKey + nextPageToken +  "&maxResults=50&order=time";
				mAnswer = mGoogleConnect->retreiveGetAsyncRequest(request.c_str(), "getCommentList", this);
				mAnswer->AddDynamicAttribute(CoreModifiable::ATTRIBUTE_TYPE::STRING, "videoID", videoID.c_str());
				mAnswer->Init();
				printf("Request : getCommentList\n ");
				myRequestCount++;
			}
			else // get authors from already saved file
			{
				mParsedComments += (int)initP["ParsedComments"];
				
				CoreItemSP authorList = initP["authors"];
				if (!authorList.isNil())
				{
					for (int i = 0; i < authorList->size(); i++)
					{
						mAuthorListToProcess.push_back(authorList[i]);
						mCurrentAuthorList.insert(authorList[i]);
					}
					mState = 3;
				}
				else
				{
					mState = 11; // retrieve more videos for this channel
				}

			}
		}
		else
		{
			mState = 11; // retrieve more videos for this channel
		}
	}
	break;
	case 3: // treat author list
	{
#ifdef LOG_ALL
		log("********************    state 3");
#endif
		mState = 0; // wait

		// check if we reached user count per video goal
		if ( (mMaxUserPerVideo && (mCurrentVideoUserFound >= mMaxUserPerVideo)) || (mBadVideo))
		{
#ifdef LOG_ALL
			log("********************* Max user for video");
#endif
			// go to next video
			mState = 2;
			mCurrentProcessedVideo++;
			mCurrentVideoUserFound = 0;
			mNotSubscribedUserForThisVideo = 0;
			mBadVideo = false;
			break;
		}

		if (mAuthorListToProcess.size())
		{

			// https://www.googleapis.com/youtube/v3/subscriptions?part=snippet%2CcontentDetails&channelId=UC_x5XG1OV2P6uZZ5FSM9Ttw&key=[YOUR_API_KEY]'
 
			mCurrentProcessedUser = *mAuthorListToProcess.begin();
			mAuthorListToProcess.erase(mAuthorListToProcess.begin());
#ifdef LOG_ALL
			log(std::string("Process user ") + mCurrentProcessedUser);
#endif
			if (mFoundUsers.find(mCurrentProcessedUser) != mFoundUsers.end())
			{
#ifdef LOG_ALL
				log("!!!!!!!!!!!! already treated user");
#endif

				printf("already treated user\n");
				mState = 3; // process next author
			}
			else
			{
				std::string filename = "Cache/Authors/";
				filename += mCurrentProcessedUser + ".json";

				CoreItemSP initP = LoadJSon(filename);

				// we don't know this writer, retrieve its subscription
				if (initP.isNil())
				{
					std::string url = "/youtube/v3/subscriptions?part=snippet%2CcontentDetails&channelId=";
					std::string request = url + mCurrentProcessedUser + mKey + "&maxResults=50";
					mAnswer = mGoogleConnect->retreiveGetAsyncRequest(request.c_str(), "getUserSubscribtion", this);
					mAnswer->AddDynamicAttribute(CoreModifiable::ATTRIBUTE_TYPE::STRING, "request", request.c_str());
					mAnswer->Init();
					printf("Request : getUserSubscribtion\n ");
					myRequestCount++;
				}
				else // load subscriptions from file
				{
					CoreItemSP channels = initP["channels"];
					mCurrentUser.mHasSubscribed = false;
					mCurrentUser.mPublicChannels.clear();
					if (!channels.isNil())
					{
						myPublicWriters++;
						for (int i = 0; i < channels->size(); i++)
						{
							std::string chanID = channels[i];

							if (chanID == mChannelID) // ok this user subscribes to analysed channel
							{
								mCurrentUser.mHasSubscribed = true;
							}
							else // don't had myChannelID
							{
								mCurrentUser.mPublicChannels.push_back(chanID);
							}
						}
					}

					if (!mCurrentUser.mHasSubscribed)
					{
						if (mCurrentUser.mPublicChannels.size())
						{
							mNotSubscribedUserForThisVideo++;
							// parsed 40 users and none has subscribed to this channel
							// so set bad video flag
							if (mNotSubscribedUserForThisVideo > 20)
							{
								mBadVideo = true;
							}
						}
						for (const auto& c : mCurrentUser.mPublicChannels)
						{
							// add new found channel
							auto found = mFoundChannels.find(c);
							if (found == mFoundChannels.end())
							{
								ChannelStruct* toAdd = new ChannelStruct();
								mFoundChannels[c]=toAdd;
								toAdd->mSubscribersCount = 0;
								toAdd->mNotSubscribedSubscribersCount = 1;
								// load channel only when mSubscribersCount is enough
								/*if (!LoadChannelStruct(c, *toAdd))
								{
									// request details
									std::string url = "/youtube/v3/channels?part=statistics,snippet&id=";
									std::string request = url + c + mKey;
									mAnswer = mGoogleConnect->retreiveGetAsyncRequest(request.c_str(), "getChannelStats", this);
									printf("Request : getChannelStats %s\n ", c.c_str());
									mAnswer->AddDynamicAttribute(CoreModifiable::ATTRIBUTE_TYPE::INT,"dontSetState", 0);
									mAnswer->AddDynamicAttribute(CoreModifiable::ATTRIBUTE_TYPE::STRING, "ID", c.c_str());
									mAnswer->Init();
									myRequestCount++;
								}*/
							}
							else
							{
								(*found).second->mNotSubscribedSubscribersCount++;
							}
						}


						mFoundUsers[mCurrentProcessedUser] = mCurrentUser;
					}
					else
					{
						mySubscribedWriters++;
						mNotSubscribedUserForThisVideo = 0;
						mCurrentVideoUserFound++;
						for (const auto& c : mCurrentUser.mPublicChannels)
						{
							// add new found channel
							auto found = mFoundChannels.find(c);
							if (found == mFoundChannels.end())
							{
								ChannelStruct* toAdd = new ChannelStruct();
								mFoundChannels[c] = toAdd;
								toAdd->mSubscribersCount = 1;
								toAdd->mNotSubscribedSubscribersCount = 0;
								// load channel only when mSubscribersCount is enough
								/*if (!LoadChannelStruct(c, *toAdd))
								{
									// request details
									std::string url = "/youtube/v3/channels?part=statistics,snippet&id=";
									std::string request = url + c + mKey;
									mAnswer = mGoogleConnect->retreiveGetAsyncRequest(request.c_str(), "getChannelStats", this);
									printf("Request : getChannelStats %s\n ", c.c_str());
									mAnswer->AddDynamicAttribute(CoreModifiable::ATTRIBUTE_TYPE::INT, "dontSetState", 0);
									mAnswer->AddDynamicAttribute(CoreModifiable::ATTRIBUTE_TYPE::STRING, "ID", c.c_str());
									mAnswer->Init();
									myRequestCount++;
								}*/
							}
							else
							{
								(*found).second->mSubscribersCount++;
								if ((*found).second->mSubscribersCount==4)
								{
									if (!LoadChannelStruct(c, *(*found).second))
									{
										// request details
										std::string url = "/youtube/v3/channels?part=statistics,snippet&id=";
										std::string request = url + c + mKey;
										mAnswer = mGoogleConnect->retreiveGetAsyncRequest(request.c_str(), "getChannelStats", this);
										printf("Request : getChannelStats %s\n ", c.c_str());
										mAnswer->AddDynamicAttribute(CoreModifiable::ATTRIBUTE_TYPE::INT, "dontSetState", 0);
										mAnswer->AddDynamicAttribute(CoreModifiable::ATTRIBUTE_TYPE::STRING, "ID", c.c_str());
										mAnswer->Init();
										myRequestCount++;
									}
								}
							}
						}
						mSubscriberList.push_back(mCurrentProcessedUser);
						mFoundUsers[mCurrentProcessedUser] = mCurrentUser;

					}
					mState = 3; // process next author
				}
			}
		}
		else // go back to comment retrieving
		{
			mState = 21;
			mCurrentAuthorList.clear();
		}
	}
	break;
	case 4: // process each subscribed channel for current user
	{
		mState = 0;
		if (mTmpUserChannels.size())
		{

			auto c = mTmpUserChannels.back();

			mTmpUserChannels.pop_back();

			// add new found channel
			auto found = mFoundChannels.find(c.first);
			if (found == mFoundChannels.end())
			{

				ChannelStruct* toAdd = new ChannelStruct();
				mFoundChannels[c.first] = toAdd;
				if (mCurrentUser.mHasSubscribed)
				{
					toAdd->mSubscribersCount = 1;
					toAdd->mNotSubscribedSubscribersCount = 0;
				}
				else
				{
					toAdd->mSubscribersCount = 0;
					toAdd->mNotSubscribedSubscribersCount = 1;
				}

				// load channel only when mSubscribersCount is enough
				// check if we already know this channel
				/*if (!LoadChannelStruct(c.first, *toAdd))
				{
					toAdd->mName = c.second.first;
					toAdd->mThumb.mURL = c.second.second;
					// request details
					std::string url = "/youtube/v3/channels?part=statistics&id=";
					std::string request = url + c.first + mKey;
					mAnswer = mGoogleConnect->retreiveGetAsyncRequest(request.c_str(), "getChannelStats", this);
					printf("Request : getChannelStats %s\n ", c.first.c_str());
					mAnswer->AddDynamicAttribute(CoreModifiable::ATTRIBUTE_TYPE::STRING, "ID", c.first.c_str());
					mAnswer->Init();
					myRequestCount++;
				}
				else
				{*/
					mState = 4; // continue mTmpUserChannels
				//}
			}
			else
			{
				if (mCurrentUser.mHasSubscribed)
				{
					(*found).second->mSubscribersCount++;
					if (!LoadChannelStruct(c.first, *(*found).second))
					{
						(*found).second->mName = c.second.first;
						(*found).second->mThumb.mURL = c.second.second;
						// request details
						std::string url = "/youtube/v3/channels?part=statistics&id=";
						std::string request = url + c.first + mKey;
						mAnswer = mGoogleConnect->retreiveGetAsyncRequest(request.c_str(), "getChannelStats", this);
						printf("Request : getChannelStats %s\n ", c.first.c_str());
						mAnswer->AddDynamicAttribute(CoreModifiable::ATTRIBUTE_TYPE::STRING, "ID", c.first.c_str());
						mAnswer->Init();
						myRequestCount++;
						break;
					}
				}
				else
				{
					(*found).second->mNotSubscribedSubscribersCount++;
				}
				mState = 4; // continue mTmpUserChannels
			}
		}
		else
		{
			mState = 3; // process next author
		}

	}
	break;
	case 10:// error
	{
#ifdef LOG_ALL
		log("\nError\n");
#endif
		if (mErrorCode == 403)
		{
			SimpleCall("RequestStateChange", "ErrorSequence.xml");
			mState = 60;
		}
		else if (mErrorCode == 405)
		{
			printf("received empty answer \n");
		}
		else
		{
			// wait no more Async task
			if (!KigsCore::Instance()->hasAsyncRequestPending())
			{
				mShowedChannels.clear();
				mNeedExit = true;
			}
		}
	}
	break;
	default:
#ifdef LOG_ALL
		log("\nunknown state\n");
#endif
		printf("unknown state");
	}


	// update graphics
	double dt = GetApplicationTimer()->GetTime() - mLastUpdate;
	if (dt > 1.0)
	{
		mLastUpdate = GetApplicationTimer()->GetTime();
		if (mMainInterface)
		{
			char textBuffer[256];
			sprintf(textBuffer, "Treated writers : %d", mFoundUsers.size());
			mMainInterface["TreatedWriters"]("Text") = textBuffer;
			sprintf(textBuffer, "Subscribed writers : %d", mySubscribedWriters);
			mMainInterface["SubscWriters"]("Text") = textBuffer;
			if (mFoundUsers.size())
			{
				sprintf(textBuffer, "Public writers : %d%%", (int)((100 * myPublicWriters) / mFoundUsers.size()));
				mMainInterface["PublicWriters"]("Text") = textBuffer;
			}
			sprintf(textBuffer, "Found Channels : %d", mFoundChannels.size());
			mMainInterface["FoundChannels"]("Text") = textBuffer;
			sprintf(textBuffer, "Parsed Comments : %d", mParsedComments);
			mMainInterface["ParsedComments"]("Text") = textBuffer;
			sprintf(textBuffer, "Youtube Data API requests : %d", myRequestCount);
			mMainInterface["RequestCount"]("Text") = textBuffer;

			// check if Channel texture was loaded

			if (mChannelInfos.mThumb.mTexture && mMainInterface["thumbnail"])
			{
				const SP<UIImage>& tmp = mMainInterface["thumbnail"];

				if (!tmp->HasTexture())
				{
					tmp->addItem(mChannelInfos.mThumb.mTexture);
					mMainInterface["thumbnail"]["ChannelName"]("Text") = mChannelInfos.mName;
				}
			}
			else if(mMainInterface["thumbnail"])
			{
				LoadChannelStruct(mChannelID, mChannelInfos,true);
			}

			refreshAllThumbs();
		}
	}
	
}

void	YoutubeAnalyser::refreshAllThumbs()
{
	if (mySubscribedWriters < 10)
	{
		return;
	}

	bool somethingChanged = false;

	float wantedpercent = mValidChannelPercent;

	if (mySubscribedWriters < mSubscribedUserCount)
	{
		wantedpercent = (mValidChannelPercent * mSubscribedUserCount)/(float)mySubscribedWriters;
	}
	
	// get all parsed channels and get the ones with more than mValidChannelPercent subscribed users
	std::vector<std::pair<ChannelStruct*,std::string>>	toShow;
	for (auto c : mFoundChannels)
	{
		if (c.second->mSubscribersCount > 1)
		{
			float percent = (float)c.second->mSubscribersCount / (float)mySubscribedWriters;
			if (percent > wantedpercent)
			{
				toShow.push_back({ c.second,c.first });
			}
		}
	}

	if (toShow.size() == 0)
	{
		return;
	}

	if (mShowInfluence) // normalize according to follower count
	{
		std::sort(toShow.begin(), toShow.end(), [this](const std::pair<ChannelStruct*, std::string>& a1, const std::pair<ChannelStruct*, std::string>& a2)
			{
				if (a1.first->mTotalSubscribers == 0)
				{
					if (a2.first->mTotalSubscribers == 0)
					{
						return a1.second > a2.second;
					}
					return false;
				}
				else if (a2.first->mTotalSubscribers == 0)
				{
					return true;
				}
				// apply Jaccard index (https://en.wikipedia.org/wiki/Jaccard_index)
				// a1 subscribers %
				float A1_percent = ((float)a1.first->mSubscribersCount / (float)mySubscribedWriters);
				// a1 intersection size with analysed channel
				float A1_a_inter_b = (float)mChannelInfos.mTotalSubscribers * A1_percent;
				// a1 union size with analysed channel 
				float A1_a_union_b = (float)mChannelInfos.mTotalSubscribers + (float)a1.first->mTotalSubscribers - A1_a_inter_b;

				// a2 subscribers %
				float A2_percent = ((float)a2.first->mSubscribersCount / (float)mySubscribedWriters);
				// a2 intersection size with analysed channel
				float A2_a_inter_b = (float)mChannelInfos.mTotalSubscribers * A2_percent;
				// a2 union size with analysed channel 
				float A2_a_union_b = (float)mChannelInfos.mTotalSubscribers + (float)a2.first->mTotalSubscribers - A2_a_inter_b;

				float k1 = A1_a_inter_b / A1_a_union_b;
				float k2 = A2_a_inter_b / A2_a_union_b;

				if (k1 == k2)
				{
					return a1.second > a2.second;
				}
				return (k1 > k2);
			}
		);
	}
	else
	{
		std::sort(toShow.begin(), toShow.end(), [](const std::pair<ChannelStruct*, std::string>& a1, const std::pair<ChannelStruct*, std::string>& a2)
			{
				if (a1.first->mSubscribersCount == a2.first->mSubscribersCount)
				{
					return a1.second > a2.second;
				}
				return (a1.first->mSubscribersCount > a2.first->mSubscribersCount);
			}
		);
	}

	// check for channels already shown / to remove

	std::unordered_map<std::string, unsigned int>	currentShowedChannels;
	int toShowCount = 0;
	for (const auto& tos : toShow)
	{
		currentShowedChannels[tos.second] = 1;
		toShowCount++;
		if (toShowCount >= mMaxChannelCount)
			break;
	}


	for (const auto& s : mShowedChannels)
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
			auto toremove = mShowedChannels.find(update.first);
			mMainInterface->removeItem((*toremove).second);
			mShowedChannels.erase(toremove);
			somethingChanged = true;
		}
		else if (update.second == 1) // to add
		{
			std::string thumbName = "thumbNail_"+ update.first;
			CMSP toAdd = CoreModifiable::Import("Thumbnail.xml", false, false, nullptr, thumbName);
			toAdd->AddDynamicAttribute<maFloat, float>("Radius", 1.0f);
			mShowedChannels[update.first] = toAdd;
			mMainInterface->addItem(toAdd);
			somethingChanged = true;
		}
	}

	float dangle = 2.0f * KFLOAT_CONST_PI / 7.0f;
	float angle = 0.0f;
	float ray = 0.15f;
	float dray = 0.0117f;
	toShowCount = 0;
	for (const auto& toPlace : toShow)
	{
		auto found=mShowedChannels.find(toPlace.second);
		if (found != mShowedChannels.end())
		{
			const CMSP& toSetup = (*found).second;
			v2f dock(0.53f + ray * cosf(angle), 0.47f + ray*1.02f * sinf(angle));
			toSetup("Dock") = dock;
			angle += dangle;
			dangle = 2.0f * KFLOAT_CONST_PI / (2.0f+50.0f*ray);
			ray += dray;
			dray *= 0.98f;
			toSetup["ChannelName"]("Text") = toPlace.first->mName;
			float prescale = 1.0f;
			if (mShowInfluence) // normalize according to follower count
			{
				
				// apply Jaccard index (https://en.wikipedia.org/wiki/Jaccard_index)
				// A subscribers %
				float A_percent = ((float)toPlace.first->mSubscribersCount / (float)mySubscribedWriters);
				// A intersection size with analysed channel
				float A_a_inter_b = (float)mChannelInfos.mTotalSubscribers * A_percent;
				// A union size with analysed channel 
				float A_a_union_b = (float)mChannelInfos.mTotalSubscribers + (float)toPlace.first->mTotalSubscribers - A_a_inter_b;

				float k = 100.0f * A_a_inter_b / A_a_union_b;
				if (k > 100.0f) // avoid Jaccard index greater than 100
				{
					k = 100.0f;
				}
				if (toPlace.first->mTotalSubscribers == 0)
				{
					k = 0.0f;
				}
				toSetup["ChannelPercent"]("Text") = std::to_string((int)(k)) + "sc";

				prescale = 1.5f * k / 100.0f;
				
			}
			else
			{
				int percent = (int)(100.0f * ((float)toPlace.first->mSubscribersCount / (float)mySubscribedWriters));
				toSetup["ChannelPercent"]("Text") = std::to_string(percent) + "%";

				prescale = percent / 100.0f;
			}

			prescale = sqrtf(prescale);
			if (prescale > 0.8f)
			{
				prescale = 0.8f;
			}
			
			// set ChannnelPercent position depending on where the thumb is in the spiral

			if ((dock.x > 0.45f) && (dock.x < 0.61f))
			{
				toSetup["ChannelPercent"]("Dock") = v2f(0.5,0.0);
				toSetup["ChannelPercent"]("Anchor") = v2f(0.5,1.0);
			}
			else if(dock.x<=0.45f)
			{
				toSetup["ChannelPercent"]("Dock") = v2f(1.0, 0.5);
				toSetup["ChannelPercent"]("Anchor") = v2f(0.0, 0.5);
			}
			else
			{
				toSetup["ChannelPercent"]("Dock") = v2f(0.0, 0.5);
				toSetup["ChannelPercent"]("Anchor") = v2f(1.0, 0.5);
			}


			toSetup("PreScaleX") = 1.2f * prescale;
			toSetup("PreScaleY") = 1.2f * prescale;

			toSetup("Radius")=(float)toSetup("SizeX")*1.2*prescale*0.5f;

			toSetup["ChannelPercent"]("FontSize") = 0.6f* 24.0f / prescale;
			toSetup["ChannelPercent"]("MaxWidth") = 0.6f * 100.0f / prescale;
			toSetup["ChannelName"]("FontSize") = 0.6f * 18.0f / prescale;
			toSetup["ChannelName"]("MaxWidth") = 0.6f * 150.0f / prescale;

			const SP<UIImage>& checkTexture = toSetup;

			if (!checkTexture->HasTexture())
			{
				somethingChanged = true;
				if (toPlace.first->mThumb.mTexture)
				{
					checkTexture->addItem(toPlace.first->mThumb.mTexture);
				}
				else
				{
					LoadChannelStruct(toPlace.second, *toPlace.first,true);
				}

			}
		}
		toShowCount++;
		if (toShowCount >= mMaxChannelCount)
			break;
	}

	if (mySubscribedWriters >= mSubscribedUserCount)
	{
		if( somethingChanged == false) 
		{
			if (mState != 50)
			{
				SaveStatFile();
				#ifdef LOG_ALL
				closeLog();
				#endif
				mState = 50; // finished
			}
			
			// clear unwanted texts when finished

			mMainInterface["ParsedComments"]("Text") = "";
			mMainInterface["RequestCount"]("Text") = "";
			mMainInterface["CurrentVideo"]("Text") = "";
			mMainInterface["switchForce"]("IsHidden") = false;
			mMainInterface["switchForce"]("IsTouchable") = true;

		}
	}
}

float	YoutubeAnalyser::PerChannelUserMap::GetNormalisedSimilitude(const PerChannelUserMap& other)
{
	unsigned int count = 0;
	for (int i = 0; i < mSize; i++)
	{
		if (other.m[i] & m[i])
		{
			count++;
		}
	}

	float result = ((float)count) / ((float)(mSubscribedCount + other.mSubscribedCount - count));

	return result;
}

float	YoutubeAnalyser::PerChannelUserMap::GetNormalisedAttraction(const PerChannelUserMap& other)
{
	unsigned int count = 0;
	for (int i = 0; i < mSize; i++)
	{
		if (other.m[i] & m[i])
		{
			count++;
		}
	}

	float minSCount = (float)std::min(mSubscribedCount, other.mSubscribedCount);

	float result = ((float)count) / minSCount;

	return result;
}

void	YoutubeAnalyser::prepareForceGraphData()
{

	Histogram<float>	hist({0.0,1.0},256);

	mChannelSubscriberMap.clear();

	// for each showed channel
	for (auto& c : mShowedChannels)
	{
		PerChannelUserMap	toAdd(mSubscriberList.size());
		int sindex = 0;
		int subcount = 0;
		for (auto& s : mSubscriberList)
		{
			if (mFoundUsers[s].isSubscriberOf(c.first))
			{
				toAdd.SetSubscriber(sindex);
			}
			++sindex;
		}
		toAdd.mThumbnail = c.second;
		c.second["ChannelPercent"]("Text") = "";
		c.second["ChannelName"]("Text") = "";
		mChannelSubscriberMap[c.first] = toAdd;
	}

	for (auto& l1 : mChannelSubscriberMap)
	{
		l1.second.mPos = l1.second.mThumbnail->getValue<v2f>("Dock");
		l1.second.mPos.x = 640+(rand()%129)-64;
		l1.second.mPos.y = 400+(rand()%81)-40;
		l1.second.mForce.Set(0.0f, 0.0f);
		l1.second.mRadius = l1.second.mThumbnail->getValue<float>("Radius");

		int index = 0;
		for (auto& l2 : mChannelSubscriberMap)
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
		std::sort(l1.second.mCoeffs.begin(), l1.second.mCoeffs.end(), [](const std::pair<int,float>& a1, const std::pair<int, float>& a2)
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

	std::vector<float> histlimits = hist.getPercentList({0.05f,0.5f,0.96f});

	
	float rangecoef1 = 1.0f / (histlimits[1] - histlimits[0]);
	float rangecoef2 = 1.0f / (histlimits[2] - histlimits[1]);
	int index1 = 0;
	for (auto& l1 : mChannelSubscriberMap)
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

void	YoutubeAnalyser::DrawForceBased()
{
	v2f center(1280.0 / 2.0, 800.0 / 2.0);

	const float timeDelay = 10.0f;
	const float timeDivisor = 0.04f;

	float currentTime = ((GetApplicationTimer()->GetTime() - mForcedBaseStartingTime) - timeDelay) * timeDivisor;
	if (currentTime > 1.0f)
	{
		currentTime = 1.0f;
	}
	// first compute attraction force on each thumb
	for (auto& l1 : mChannelSubscriberMap)
	{
		PerChannelUserMap& current = l1.second;
	
		// always a  bit of attraction to center
		v2f	v(mThumbcenter);
		v -= current.mPos;

		// add a bit of random
		v.x += (rand() % 32) - 16;
		v.y += (rand() % 32) - 16;
		
		current.mForce *= 0.1f;
		current.mForce=v*0.001f;
	
		int i = 0;
		for (auto& l2 : mChannelSubscriberMap)
		{
			if (&l1 == &l2)
			{
				i++;
				continue;
			}
			
			v2f	v(l2.second.mPos - current.mPos);
			float dist = Norm(v);
			v.Normalize();
			float coef = current.mCoeffs[i].second;
			if (currentTime < 0.0f)
			{
				coef += -currentTime / (timeDelay* timeDivisor) ;
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
	for (auto& l1 : mChannelSubscriberMap)
	{
		PerChannelUserMap& current = l1.second;
		current.mPos += current.mForce;
	}

	if (currentTime > 0.0f)
	{
		// then adjust position with contact
		for (auto& l1 : mChannelSubscriberMap)
		{
			PerChannelUserMap& current = l1.second;

			for (auto& l2 : mChannelSubscriberMap)
			{
				if (&l1 == &l2)
				{
					/*// check with central image
					v2f	v(current.mPos);
					v -= thumbcenter;
					float dist = Norm(v);
					v.Normalize();

					float r = (current.mRadius + 120) * currentTime;
					if (dist < r)
					{
						float coef = (r - dist) * 120.0 / (r);
						current.mPos += v * (coef);
					}
					*/
					continue;
				}

				v2f	v(l2.second.mPos - current.mPos);
				float dist = Norm(v);
				v.Normalize();

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
	for (auto& l1 : mChannelSubscriberMap)
	{

		PerChannelUserMap& current = l1.second;
		v2f	v(current.mPos);
		v -= center;
		float l1 = Norm(v);
		
		float angle = atan2f(v.y, v.x);
		v.Normalize();

		v2f ellipse(center);
		ellipse.x *= 0.9*cosf(angle);
		ellipse.y *= 0.9*sinf(angle);

		v2f tst = ellipse.Normalized();

		ellipse -= tst * current.mRadius;
		float l2 = Norm(ellipse);

		if (l1 > l2)
		{
			current.mPos -= v*(l1-l2);
		}

		v2f dock(current.mPos);
		dock.x /= 1280.0f;
		dock.y /= 800.0f;

		current.mThumbnail("Dock") = dock;
	}


}

void		YoutubeAnalyser::SaveStatFile()
{

	std::string filename = mChannelInfos.mName.ToString();
	filename += "_stats.csv";

	// avoid slash and backslash
	std::replace(filename.begin(), filename.end(), '/', ' ');
	std::replace(filename.begin(), filename.end(), '\\', ' ');

	filename = FilePathManager::MakeValidFileName(filename);

	// get all parsed channels and get the ones with more than mValidChannelPercent subscribed users
	std::vector<std::pair<ChannelStruct*, std::string>>	toSave;
	for (auto c : mFoundChannels)
	{
		if (c.second->mSubscribersCount >= 1)
		{
			float percent = (float)c.second->mSubscribersCount / (float)mySubscribedWriters;
			if (percent > mValidChannelPercent)
			{
				toSave.push_back({ c.second,c.first });
			}
		}
	}

	std::sort(toSave.begin(), toSave.end(), [](const std::pair<ChannelStruct*, std::string>& a1, const std::pair<ChannelStruct*, std::string>& a2)
		{
			if (a1.first->mSubscribersCount == a2.first->mSubscribersCount)
			{
				return a1.second > a2.second;
			}
			return (a1.first->mSubscribersCount > a2.first->mSubscribersCount);
		}
	);

	std::vector<std::pair<ChannelStruct*, std::string>>	toSaveUnsub;
	float unsubWriters = myPublicWriters -mySubscribedWriters;
	for (auto c : mFoundChannels)
	{
		if (c.second->mNotSubscribedSubscribersCount >= 1)
		{
			float percent = (float)c.second->mNotSubscribedSubscribersCount / unsubWriters;
			if (percent > mValidChannelPercent)
			{
				toSaveUnsub.push_back({ c.second,c.first });
			}
		}
	}

	std::sort(toSaveUnsub.begin(), toSaveUnsub.end(), [](const std::pair<ChannelStruct*, std::string>& a1, const std::pair<ChannelStruct*, std::string>& a2)
		{
			if (a1.first->mNotSubscribedSubscribersCount == a2.first->mNotSubscribedSubscribersCount)
			{
				return a1.second > a2.second;
			}
			return (a1.first->mNotSubscribedSubscribersCount > a2.first->mNotSubscribedSubscribersCount);
		}
	);

	SmartPointer<::FileHandle> L_File = Platform_fopen(filename.c_str(), "wb");
	if (L_File->mFile)
	{
		// save general stats
		char saveBuffer[4096];
		sprintf(saveBuffer, "Total writers found,%d\n", mFoundUsers.size());
		Platform_fwrite(saveBuffer, 1, strlen(saveBuffer), L_File.get());
		sprintf(saveBuffer, "Public writers found,%d\n", myPublicWriters);
		Platform_fwrite(saveBuffer, 1, strlen(saveBuffer), L_File.get());
		sprintf(saveBuffer, "Subscribed writers found,%d\n", mySubscribedWriters);
		Platform_fwrite(saveBuffer, 1, strlen(saveBuffer), L_File.get());
		sprintf(saveBuffer, "Found channels Count,%d\n", mFoundChannels.size());
		Platform_fwrite(saveBuffer, 1, strlen(saveBuffer), L_File.get());

		std::string header = "Channel Name,Channel ID,Percent,Jaccard\n";
		Platform_fwrite(header.c_str(), 1, header.length(), L_File.get());
		
		for (const auto& p : toSave)
		{
			int percent = (int)(100.0f * ((float)p.first->mSubscribersCount / (float)mySubscribedWriters));
			// A subscribers %
			float A_percent = ((float)p.first->mSubscribersCount / (float)mySubscribedWriters);
			// A intersection size with analysed channel
			float A_a_inter_b = (float)mChannelInfos.mTotalSubscribers * A_percent;
			// A union size with analysed channel 
			float A_a_union_b = (float)mChannelInfos.mTotalSubscribers + (float)p.first->mTotalSubscribers - A_a_inter_b;

			int J = (int)(100.0f * A_a_inter_b / A_a_union_b);
			sprintf(saveBuffer, "\"%s\",%s,%d,%d\n",p.first->mName.ToString().c_str(),p.second.c_str(), percent,J);
			Platform_fwrite(saveBuffer, 1, strlen(saveBuffer), L_File.get());
		}
		
		header = "\nNot subscribed writers\nChannel Name,Channel ID,Percent,J\n";
		Platform_fwrite(header.c_str(), 1, header.length(), L_File.get());

		for (const auto& p : toSaveUnsub)
		{
			int percent = (int)(100.0f * ((float)p.first->mNotSubscribedSubscribersCount / (float)unsubWriters));
			// A subscribers %
			float A_percent = ((float)p.first->mSubscribersCount / (float)mySubscribedWriters);
			// A intersection size with analysed channel
			float A_a_inter_b = (float)mChannelInfos.mTotalSubscribers * A_percent;
			// A union size with analysed channel 
			float A_a_union_b = (float)mChannelInfos.mTotalSubscribers + (float)p.first->mTotalSubscribers - A_a_inter_b;

			int J = (int)(100.0f * A_a_inter_b / A_a_union_b);


			sprintf(saveBuffer, "\"%s\",%s,%d,%d\n", p.first->mName.ToString().c_str(), p.second.c_str(), percent,J);
			Platform_fwrite(saveBuffer, 1, strlen(saveBuffer), L_File.get());
		}

		Platform_fclose(L_File.get());
	}
}

void	YoutubeAnalyser::ProtectedClose()
{
	DataDrivenBaseApplication::ProtectedClose();
	CoreDestroyModule(HTTPRequestModule);
}

void	YoutubeAnalyser::ProtectedInitSequence(const kstl::string& sequence)
{
	if (sequence == "sequencemain")
	{
		mMainInterface = GetFirstInstanceByName("UIItem", "Interface");
		mMainInterface["switchForce"]("IsHidden") = true;
		mMainInterface["switchForce"]("IsTouchable") = false;
	}
}
void	YoutubeAnalyser::ProtectedCloseSequence(const kstl::string& sequence)
{
	if (sequence == "sequencemain")
	{
		mMainInterface = nullptr;

		// delete all channel structs

		for (const auto& c : mFoundChannels)
		{
			delete c.second;
		}

		mFoundChannels.clear();
		mShowedChannels.clear();
		mChannelInfos.mThumb.mTexture = nullptr;
		mDownloaderList.clear();
		mChannelSubscriberMap.clear();


#ifdef LOG_ALL
		closeLog();
#endif
	}
}


CoreItemSP	YoutubeAnalyser::RetrieveJSON(CoreModifiable* sender)
{
	void* resultbuffer = nullptr;
	sender->getValue("ReceivedBuffer", resultbuffer);

	if (resultbuffer)
	{
		CoreRawBuffer* r = (CoreRawBuffer*)resultbuffer;
		std::string_view received(r->data(), r->size());

		std::string validstring(received);

		if (validstring.length() == 0)
		{
			mState = 10;
			mErrorCode = 405;
		}
		else
		{

			usString	utf8string((UTF8Char*)validstring.c_str());

			JSonFileParserUTF16 L_JsonParser;
			CoreItemSP result = L_JsonParser.Get_JsonDictionaryFromString(utf8string);

			if (!result["error"].isNil())
			{
				std::string reason = result["error"]["errors"][0]["reason"];
				int code = result["error"]["code"];
				if (code == 403)
				{
					if ((reason == "quotaExceeded") || (reason == "dailyLimitExceeded"))
					{
						mState = 10;
						mErrorCode = code;
					}
				}
			}


			if (mState != 10)
			{
				return result;
			}
		}
	}

	return nullptr;
}

CoreItemSP	YoutubeAnalyser::LoadJSon(const std::string& fname,bool utf16)
{
	auto& pathManager = KigsCore::Singleton<FilePathManager>();
	SmartPointer<::FileHandle> filenamehandle = pathManager->FindFullName(fname);

	// Windows specific code 
#ifdef WIN32

	if ((filenamehandle->mStatus & FileHandle::Exist) && (mOldFileLimit>1.0))
	{
		struct _stat resultbuf;

		if (_stat(filenamehandle->mFullFileName.c_str(), &resultbuf) == 0)
		{
			auto mod_time = resultbuf.st_mtime;

			double diffseconds= difftime(mCurrentTime, mod_time);

			if (diffseconds > mOldFileLimit)
			{
				return nullptr;
			}
		}
	}


#endif
	CoreItemSP initP;
	if (utf16)
	{
		JSonFileParserUTF16 L_JsonParser;
		initP = L_JsonParser.Get_JsonDictionary(filenamehandle);
	}
	else
	{
		JSonFileParser L_JsonParser;
		initP = L_JsonParser.Get_JsonDictionary(filenamehandle);
	}
	return initP;
}


void	YoutubeAnalyser::thumbnailReceived(CoreRawBuffer* data, CoreModifiable* downloader)
{
	
	// search used downloader in vector
	std::vector<std::pair<CMSP, std::pair<std::string,ChannelStruct*>> > tmpList;

	for (const auto& p : mDownloaderList)
	{
		if (p.first.get() == downloader)
		{
			TinyImage* img = new JPEGClass(data);
			if (img)
			{
				ChannelStruct* toFill = p.second.second;

				std::string filename= "Cache/Thumbs/";
				filename += p.second.first;
				filename += ".jpg";
				
				// export image
				SmartPointer<::FileHandle> L_File = Platform_fopen(filename.c_str(), "wb");
				if (L_File->mFile)
				{
					Platform_fwrite(data->buffer(),1, data->length(), L_File.get());
					Platform_fclose(L_File.get());
				}

				toFill->mThumb.mTexture = KigsCore::GetInstanceOf((std::string)toFill->mName + "tex", "Texture");
				toFill->mThumb.mTexture->Init();

				SmartPointer<TinyImage>	imgsp = OwningRawPtrToSmartPtr(img);
				toFill->mThumb.mTexture->CreateFromImage(imgsp);
			}
		}
		else
		{
			tmpList.push_back(p);
		}
	}
	mDownloaderList = std::move(tmpList);
}

DEFINE_METHOD(YoutubeAnalyser, getChannelID)
{
	auto json=RetrieveJSON(sender);

	if (!json.isNil())
	{
		CoreItemSP IDItem = json["items"][0]["id"];

		if (!IDItem.isNil())
		{
			ChannelStruct* currentChan = nullptr;
			bool isMainChannel=false;
			if (mChannelID == "") // main channel
			{
				isMainChannel = true;
				mChannelID = IDItem;
				currentChan = &mChannelInfos;

				// save channel id
				{
					JSonFileParser L_JsonParser;
					CoreItemSP initP = CoreItemSP::getCoreMap();
					initP->set("channelID", CoreItemSP::getCoreValue(mChannelID));

					std::string filename = "Cache/" + mChannelName + "/";
					filename += mChannelName + ".json";
					L_JsonParser.Export((CoreMap<std::string>*)initP.get(), filename);
				}
			}
			else
			{
				auto f = mFoundChannels.find((std::string)IDItem);
				if (f != mFoundChannels.end())
				{
					currentChan = (*f).second;
				}
			}

			if (currentChan)
			{
				if (!LoadChannelStruct(IDItem, *currentChan,true))
				{
					CoreItemSP infos = json["items"][0]["snippet"];
					currentChan->mName = infos["title"];
					currentChan->mTotalSubscribers = json["items"][0]["statistics"]["subscriberCount"];
					std::string imgurl = infos["thumbnails"]["default"]["url"];
					currentChan->mThumb.mURL = imgurl;
					SaveChannelStruct(IDItem, *currentChan);
					LaunchDownloader(IDItem, *currentChan);
				}
				if(isMainChannel)
					mState = 1;

				return true;
			}

		}
	}
	mState = 10; // Error
	return true;
}

DEFINE_METHOD(YoutubeAnalyser, getChannelStats)
{
	if (mState >= 10)
	{
		return true;
	}
	auto json = RetrieveJSON(sender);

	if (!json.isNil())
	{
		if (!json["items"][0]["id"].isNil())
		{
			std::string id = json["items"][0]["id"];

			ChannelStruct* current = mFoundChannels[id];

			CoreItemSP infos = json["items"][0]["snippet"];
			if (!infos.isNil())
			{
				current->mName = infos["title"];
				current->mThumb.mURL = infos["thumbnails"]["default"]["url"];
			}
			current->mTotalSubscribers = 0;
			CoreItemSP subscount = json["items"][0]["statistics"]["subscriberCount"];
			if (!subscount.isNil())
			{
				current->mTotalSubscribers = subscount;
			}
			
			SaveChannelStruct(id, *current);
		}
		else // data unavailable, save "empty" file
		{
			std::string id;
			sender->getValue("ID", id);
			ChannelStruct* current = mFoundChannels[id];
			SaveChannelStruct(id, *current);
		}
		int nextState = 0;
		if (sender->getValue("dontSetState", nextState))
		{
			// if nextState is > 0 then set mState using nextState
			if (nextState > 0)
			{
				mState = nextState;
			}
		
		}
		else
		{
			mState = 4;
		}
		return true;
	}
	mState = 10; // Error
	return true;
}

void		YoutubeAnalyser::SaveAuthorList(const std::string& nextPage,const std::string& videoID, unsigned int localParsedComments)
{
	std::string filename = "Cache/" + mChannelName + "/videos/";

	filename += videoID + "_videos.json";

	CoreItemSP initP = LoadJSon(filename);


	if (initP.isNil())
	{
		initP = CoreItemSP::getCoreMap();
	}

	if (nextPage != "")
	{
		initP->set("nextPageToken", CoreItemSP::getCoreValue(nextPage));
	}
	else
	{
		initP->erase("nextPageToken");
	}

	CoreItemSP v = initP["authors"];
	if (v.isNil())
	{
		v = CoreItemSP::getCoreVector();
	}

	// add new authors
	for (const auto& auth : mAuthorListToProcess)
	{
		v->set("", CoreItemSP::getCoreValue(auth));
	}

	initP->set("authors", v);
	
	CoreItemSP parsedComments = initP["ParsedComments"];
	if (parsedComments.isNil())
	{
		parsedComments = CoreItemSP::getCoreValue(0);
	}
	parsedComments = ((int)parsedComments) + localParsedComments;
	
	initP->set("ParsedComments", parsedComments);

	JSonFileParser L_JsonParser;
	L_JsonParser.Export((CoreMap<std::string>*)initP.get(), filename);
}

void YoutubeAnalyser::SaveVideoList(const std::string& nextPage)
{

	std::string filename = "Cache/" + mChannelName + "/videos/";
	filename += mChannelID + ".json";

	CoreItemSP initP = LoadJSon(filename);

	if (initP.isNil())
	{
		initP = CoreItemSP::getCoreMap();
	}

	if (nextPage != "")
	{
		initP->set("nextPageToken", CoreItemSP::getCoreValue(nextPage));
	}
	else
	{
		initP->erase("nextPageToken");
	}

	CoreItemSP v = CoreItemSP::getCoreVector();
	// titles
	CoreItemSP nv = CoreItemSP::getCoreVector();

	// mVideoListToProcess is never reset, so just write the full list
	for (const auto& vid : mVideoListToProcess)
	{
		v->set("", CoreItemSP::getCoreValue(vid.first));
		nv->set("", CoreItemSP::getCoreValue(vid.second));
	}

	initP->set("videos", v);
	initP->set("videosTitles", nv);
	JSonFileParser L_JsonParser;
	L_JsonParser.Export((CoreMap<std::string>*)initP.get(), filename);

}

void		YoutubeAnalyser::SaveAuthor(const std::string& id)
{
	JSonFileParser L_JsonParser;
	CoreItemSP initP = CoreItemSP::getCoreMap();

	auto it = mFoundUsers.find(id);
	if (it != mFoundUsers.end())
	{
		UserStruct& currentUser = (*it).second;
		CoreItemSP channelList = CoreItemSP::getCoreVector();

		for (const auto& c : currentUser.mPublicChannels)
		{
			channelList->set("", CoreItemSP::getCoreValue(c));
		}

		if (currentUser.mHasSubscribed)
		{
			channelList->set("", CoreItemSP::getCoreValue(mChannelID));
		}
			
		if(channelList->size())
		{
			initP->set("channels", channelList);
		}

		std::string filename = "Cache/Authors/";
		filename += id + ".json";

		L_JsonParser.Export((CoreMap<std::string>*)initP.get(), filename);
	}

}

void		YoutubeAnalyser::SaveChannelStruct(const std::string& id, const ChannelStruct& ch)
{
	JSonFileParserUTF16 L_JsonParser;
	CoreItemSP initP = CoreItemSP::getCoreItemOfType<CoreMap<usString>>();
	initP->set("Name",CoreItemSP::getCoreValue(ch.mName));
	initP->set("TotalSubscribers", CoreItemSP::getCoreValue((int)ch.mTotalSubscribers));
	if(ch.mThumb.mURL != "")
	{
		initP->set("ImgURL", CoreItemSP::getCoreValue((usString)ch.mThumb.mURL));
	}

	std::string folderName = id.substr(0, 4);
	str_toupper(folderName);

	std::string filename = "Cache/Channels/";
	filename += folderName + "/" + id + ".json";

	L_JsonParser.Export((CoreMap<usString>*)initP.get(),filename);
}

// load json channel file dans fill ChannelStruct
bool		YoutubeAnalyser::LoadChannelStruct(const std::string& id, ChannelStruct& ch, bool requestThumb)
{
	std::string filename = "Cache/Channels/";

	std::string folderName = id.substr(0, 4);
	str_toupper(folderName);

	filename += folderName + "/" + id + ".json";

	CoreItemSP initP = LoadJSon(filename,true);

	if (initP.isNil()) // file not found, return
	{
		return false;
	}
	
	ch.mName = initP["Name"];
	ch.mTotalSubscribers = initP["TotalSubscribers"];
	if (!initP["ImgURL"].isNil())
	{
		ch.mThumb.mURL = initP["ImgURL"];
	}
	if (requestThumb && !ch.mThumb.mTexture)
	{
		
		filename = "Cache/Thumbs/";
		filename += id;
		filename += ".jpg";

		auto& pathManager = KigsCore::Singleton<FilePathManager>();
		SmartPointer<::FileHandle> fullfilenamehandle = pathManager->FindFullName(filename);

		if (fullfilenamehandle->mStatus & FileHandle::Exist)
		{
			auto& textureManager = KigsCore::Singleton<TextureFileManager>();
			ch.mThumb.mTexture = textureManager->GetTexture(filename);
		}
		else
		{
			if (ch.mThumb.mURL != "")
			{
				LaunchDownloader(id, ch);
			}
		}
	}
	return true;
}


DEFINE_METHOD(YoutubeAnalyser, getVideoList)
{

	auto json = RetrieveJSON(sender);

	std::string nextPageToken;

	if (!json.isNil())
	{
		if (!json["nextPageToken"].isNil())
		{
			nextPageToken = json["nextPageToken"];
		}

		CoreItemSP videoList = json["items"];

		if (!videoList.isNil())
		{
			if (videoList->size())
			{
				for (int i = 0; i < videoList->size(); i++)
				{
					CoreItemSP video = videoList[i];
					if (!video["id"]["kind"].isNil())
					{
						std::string kind = video["id"]["kind"];
						if (kind == "youtube#video")
						{
							if (!video["id"]["videoId"].isNil())
							{

								std::string cid = video["snippet"]["channelId"];
								if ((cid != mChannelID) || (mUseKeyword==""))
								{
									mVideoListToProcess.push_back({ (std::string)video["id"]["videoId"],(std::string)video["snippet"]["title"] });
									
								}
							}
						}
					}
				}
			}

			// save list state
			{
				SaveVideoList(nextPageToken);
			}

			mState = 2; // process video

			return true;
		}
	}

	mState = 10; // error
	
	return true;
}

DEFINE_METHOD(YoutubeAnalyser, getCommentList)
{
	auto json = RetrieveJSON(sender);
	std::string nextPageToken;

	if (!json.isNil())
	{
		if (!json["nextPageToken"].isNil())
		{
			nextPageToken = json["nextPageToken"];
		}
		CoreItemSP commentThreads = json["items"];

		unsigned int localParsedComments=0;

		if (!commentThreads.isNil())
		{
			if (commentThreads->size())
			{
				for (int i = 0; i < commentThreads->size(); i++)
				{
					CoreItemSP topComment = commentThreads[i]["snippet"]["topLevelComment"]["snippet"];
					if (!topComment.isNil())
					{
						localParsedComments++;
					
						CoreItemSP authorChannelID = topComment["authorChannelId"]["value"];
						if (!authorChannelID.isNil())
						{
							std::string authorID = authorChannelID;

							// check if not already processed
							if (mFoundUsers.find(authorID) == mFoundUsers.end())
							{
								if (mCurrentAuthorList.find(authorID) == mCurrentAuthorList.end())
								{
									mAuthorListToProcess.push_back(authorID);
									mCurrentAuthorList.insert(authorID);
								}
							}
						}
					}
					int answerCount = commentThreads[i]["snippet"]["totalReplyCount"];
					if (answerCount)
					{
						for (int j = 0; j < answerCount; j++)
						{
							CoreItemSP replies = commentThreads[i]["replies"]["comments"][j]["snippet"];
							if (!replies.isNil())
							{
								localParsedComments++;
								CoreItemSP authorChannelID = replies["authorChannelId"]["value"];
								if (!authorChannelID.isNil())
								{
									std::string authorID = authorChannelID;

									// check if not already processed
									if (mFoundUsers.find(authorID) == mFoundUsers.end())
									{
										if (mCurrentAuthorList.find(authorID) == mCurrentAuthorList.end())
										{
											mAuthorListToProcess.push_back(authorID);
											mCurrentAuthorList.insert(authorID);
										}
									}
								}
							}
						}
					}
				}
			}
		}
		// save authors state
		{
			std::string id = sender->getValue<std::string>("videoID");
			SaveAuthorList(nextPageToken,id, localParsedComments);
		}

		mParsedComments += localParsedComments;

		mState = 3; // process author
		return true;
	}

	mState = 10;
	return true;
}

DEFINE_METHOD(YoutubeAnalyser, getUserSubscribtion)
{
	bool isNewPage=false;
	sender->getValue("NewPage", isNewPage);
	
	if (!isNewPage) // first page
	{
		mCurrentUser.mHasSubscribed = false;
		mCurrentUser.mPublicChannels.clear();
		mTmpUserChannels.clear();
	}

	std::string nextPageToken;

	auto json = RetrieveJSON(sender);

	bool isLastPage = true;

	if (!json.isNil())
	{
		if (!json["nextPageToken"].isNil())
		{
			isLastPage = false;
			nextPageToken = json["nextPageToken"];
		}

		CoreItemSP subscriptions = json["items"];
		if (!subscriptions.isNil())
		{
			if (subscriptions->size())
			{
				for (int i = 0; i < subscriptions->size(); i++)
				{
					CoreItemSP subscription = subscriptions[i]["snippet"]["resourceId"]["channelId"];
					if (!subscription.isNil())
					{

						usString title = subscriptions[i]["snippet"]["title"];
						CoreItemSP thumbnailInfos = subscriptions[i]["snippet"]["thumbnails"];
						std::string url="";
						if (!thumbnailInfos.isNil())
						{
							url = thumbnailInfos["default"]["url"];
						} 
						std::string chanID = subscription;

						if (chanID == mChannelID) // ok this user subscribes to analysed channel
						{
							mCurrentUser.mHasSubscribed = true;
						}
						else // don't had myChannelID
						{
							mTmpUserChannels.push_back({ chanID,{title,url} });
						}
					}
				}
			}
		}
	}
	if (isLastPage)
	{
		bool isPublicWriter = false;
		if (mTmpUserChannels.size())
		{
			myPublicWriters++;
			isPublicWriter = true;
		}
		if (mCurrentUser.mHasSubscribed)
		{
			mNotSubscribedUserForThisVideo = 0;
			mySubscribedWriters++;
			mCurrentVideoUserFound++;
			for (const auto& c : mTmpUserChannels)
			{
				mCurrentUser.mPublicChannels.push_back(c.first);

			}
			mSubscriberList.push_back(mCurrentProcessedUser);
			mFoundUsers[mCurrentProcessedUser] = mCurrentUser;

		}
		else  // add user without subscription
		{
			if (isPublicWriter)
			{
				mNotSubscribedUserForThisVideo++;
				// parsed 40 users and none has subscribed to this channel
				// so set bad video flag
				if (mNotSubscribedUserForThisVideo > 20)
				{
					mBadVideo = true;
				}
			}
			for (const auto& c : mTmpUserChannels)
			{
				mCurrentUser.mPublicChannels.push_back(c.first);
			}
			mFoundUsers[mCurrentProcessedUser] = mCurrentUser;
		}
		SaveAuthor(mCurrentProcessedUser);

		mState = 4; // process mTmpUserChannels
	}
	else
	{
		// call it again
		std::string request;
		sender->getValue("request", request);
		std::string requestPage = request + "&pageToken=" + nextPageToken;
		mAnswer = mGoogleConnect->retreiveGetAsyncRequest(requestPage.c_str(), "getUserSubscribtion", this);
		mAnswer->AddDynamicAttribute(CoreModifiable::ATTRIBUTE_TYPE::STRING, "request", request.c_str());
		mAnswer->AddDynamicAttribute(CoreModifiable::ATTRIBUTE_TYPE::BOOL, "NewPage", true);
		mAnswer->Init();
		printf("Request : getUserSubscribtion again\n ");
		myRequestCount++;
	}
	
	return true;
}

void		YoutubeAnalyser::LaunchDownloader(const std::string& id, ChannelStruct& ch)
{
	// first, check that downloader for the same thumb is not already launched

	for (const auto& d : mDownloaderList)
	{
		if (d.second.first == id) // downloader found
		{
			return;
		}
	}

	if (ch.mThumb.mURL == "") // need a valid url
	{
		return;
	}

	CMSP CurrentDownloader = KigsCore::GetInstanceOf("downloader", "ResourceDownloader");
	CurrentDownloader->setValue("URL", ch.mThumb.mURL);
	KigsCore::Connect(CurrentDownloader.get(), "onDownloadDone", this, "thumbnailReceived");

	mDownloaderList.push_back({ CurrentDownloader , {id,&ch} });

	CurrentDownloader->Init();
}

void	YoutubeAnalyser::switchDisplay()
{
	mMainInterface["switchForce"]("IsHidden") = true;
	mMainInterface["switchForce"]("IsTouchable") = false;

	mShowInfluence = !mShowInfluence;
}

void	YoutubeAnalyser::switchForce()
{
	if (!mDrawForceBased)
	{
		mThumbcenter = (v2f)mMainInterface["thumbnail"]("Dock");
		mThumbcenter.x *= 1280.0f;
		mThumbcenter.y *= 800.0f;

		mMainInterface["thumbnail"]("Dock") = v2f(0.94f, 0.08f);

		prepareForceGraphData();
		mForcedBaseStartingTime= GetApplicationTimer()->GetTime();

		mMainInterface["switchV"]("IsHidden") = true;
		mMainInterface["switchV"]("IsTouchable") = false;
		mMainInterface["switchForce"]("Dock") = v2f(0.050f, 0.950f);
	}
	else
	{
		mMainInterface["thumbnail"]("Dock") = v2f(0.52f, 0.44f);

		mMainInterface["switchV"]("IsHidden") = false;
		mMainInterface["switchV"]("IsTouchable") = true;
		mMainInterface["switchForce"]("IsHidden") = true;
		mMainInterface["switchForce"]("IsTouchable") = false;
		mMainInterface["switchForce"]("Dock") = v2f(0.950f, 0.050f);
	}
	mDrawForceBased = !mDrawForceBased;

}