![image](https://media.discordapp.net/attachments/915012763263316019/962263569267822602/catts_banner_1000.png)



## Feature overview
- High quality neural TTS voices from Azure Cognitive Services Speech Service
- Automatically retrieve Twitch OAuth token and channel name from BeatSaberPlus (convenient for Beat Saber streamers)
- Option to not read out URLs in chat
- Option to not repeat username if the same user sends multiple messages in a row
- Limit amount of chat emotes read
- Nicknames for users
- Custom voices per user
- Replace words in chat messages
- Mute specific users
- Read channel point redemptions
- Sound Effects feature can play a sound file on specific redeems
- Automatically shoutout raiders (sends the !so command in chat)
- Change volume and sound output device for TTS and sound effects separately
- Low resource use compared to browser based solutions
- 28 KB standalone exe, no other files required besides a config file which is automatically created

https://user-images.githubusercontent.com/45233053/161430141-5a7db369-a013-41e2-b647-1566b48c6442.mp4

## Quickstart

### Prerequisites
- [Create an Azure account](https://azure.microsoft.com/en-us/pricing/purchase-options/pay-as-you-go/)  
<sup>(Note: you need to enter credit card details but you won't have to pay anything as long as you stay within free service limits. I've personally used this TTS for over a year in all my streams and never had to pay anything.)</sup>
- [Create a Speech resource](https://ms.portal.azure.com/#create/Microsoft.CognitiveServicesSpeechServices) in the Azure portal. You can use the free pricing tier (F0) which gives you [500,000 characters free per month](https://azure.microsoft.com/en-us/pricing/details/cognitive-services/speech-services/).
- View the resource and navigate to "Keys and Endpoint" / "Manage keys" to get the required key and region name
- [Here is a 1 minute video](https://www.youtube.com/watch?v=ZHtp69Vn6Oc) showing the whole process


### Setup
- Download the application from [Releases](https://github.com/catsethecat/caTTS/releases) or build it from source code  
<sup>(Note: if you download a release it will most likely get flagged as a virus, it happens because this is not a well-known/popular program. If you don't trust it you can build it yourself easily, all you need to do is have Visual Studio installed with the "Desktop Development with C++" package and then run build.bat)</sup>
- Run it and click the **Config** button. It should open the configuration file in a text editor.
- The Azure section requires your Speech resource key and region. Twitch section requires your numerical Channel ID. OauthToken and ChannelName are required if BSPlusConfig is not valid. Rest of the config options should be self explanatory.  
<sup>(Note: if you need to find your channel ID or get a token, just use google, there should be online tools available. Make sure the token has permissions to read chat and redeems.)</sup>
- Save the configuration file and click the **Reload** button in caTTS.
- If the configuration is correct and everything works then both thread statuses should say **listening**
- A list of available voices can be found [here](https://docs.microsoft.com/en-us/azure/cognitive-services/speech-service/language-support?tabs=speechtotext#prebuilt-neural-voices)
