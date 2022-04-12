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
- Minimalist software, low resource use compared to browser based solutions
- Single standalone exe, no other files required besides a config file which is automatically created

https://user-images.githubusercontent.com/45233053/161430141-5a7db369-a013-41e2-b647-1566b48c6442.mp4

## Quickstart

### Prerequisites
- Azure subscription - [Create one for free](https://azure.microsoft.com/free/cognitive-services)
- [Create a Speech resource](https://ms.portal.azure.com/#create/Microsoft.CognitiveServicesSpeechServices) in the Azure portal. You can use the free pricing tier (F0) which gives you 500,000 characters free per month.
- Get the subscription key and region by viewing the resource in Azure Portal and managing keys

### Setup
- Download the application from [Releases](https://github.com/catsethecat/caTTS/releases) or build it from source code
- Run it and click the **Config** button. It should open the configuration file in a text editor.
- The Azure section requires your subscription key and region. Twitch section requires your Channel ID. OauthToken and ChannelName are required if BSPlusConfig is not valid. Rest of the config options should be self explanatory.
- Save the configuration file and click the **Reload** button in caTTS.
- If the configuration is correct and everything works then both thread statuses should say **listening**
- A list of available voices can be found [here](https://docs.microsoft.com/en-us/azure/cognitive-services/speech-service/language-support?tabs=speechtotext#prebuilt-neural-voices)
