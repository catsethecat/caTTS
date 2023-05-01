![image](https://cdn.discordapp.com/attachments/852088618594992159/1065989914166050877/catts_banner_1000.png)

## Feature overview
- High quality neural TTS voices from Azure Cognitive Services Speech Service
- Option to not read out URLs in chat
- Option to not repeat username if the same user sends multiple messages in a row
- Limit amount of chat emotes read
- Limit message length and TTS audio length
- Nicknames for users
- Custom voices per user
- Custom SSML support for using different speaking styles
- Replace words in chat messages
- Mute specific users
- Read channel point redemptions
- Sound Effects feature can play a sound file on specific redeems
- Automatically shoutout raiders (sends the !so command in chat)
- Change volume and sound output device for TTS and sound effects separately
- Records and displays monthly billable character count
- Option to limit monthly character usage to stay within free service limits
- Probably more optimized than any other Azure based TTS client
- 47 KB standalone exe, no other files required besides a config file which is automatically created

## Getting started

### Prerequisites
- [Create an Azure account](https://azure.microsoft.com/en-us/pricing/purchase-options/pay-as-you-go/)  
<sup>(Note: you need to enter payment details but you won't have to pay anything as long as you stay within free service limits.)</sup>
- [Create a Speech resource](https://ms.portal.azure.com/#create/Microsoft.CognitiveServicesSpeechServices) in the Azure portal. You can use the free pricing tier (F0) which gives you [500,000 characters free per month](https://azure.microsoft.com/en-us/pricing/details/cognitive-services/speech-services/).
- View the resource and navigate to "Keys and Endpoint" / "Manage keys" to get the required key and region name  
<sup>(Note: the correct region name is lowercase and contains no spaces)</sup>
- [Here is a 1 minute video](https://www.youtube.com/watch?v=ZHtp69Vn6Oc) showing the whole process


### Setup
- Download the application from [Releases](https://github.com/catsethecat/caTTS/releases) or build it from source code  
<sup>(Note: if you download a release it will most likely get flagged as a virus, it happens because this is not a well-known/popular program. If you don't trust it you can build it yourself easily, all you need to do is have Visual Studio installed with the "Desktop Development with C++" package and then run build.bat)</sup>
- Run it and it should take you through a process of authorizing the application with your Twitch account via web browser
- Right click the tray icon and click Config to open the folder containing the configuration file. Open caTTs.ini in a text editor and update the SubscriptionKey and Region in the Azure section (The [Prerequisites](#prerequisites) section shows how to get these)
- Save the changes to the file and restart the application and everything should work!
- A list of available voices can be found [here](https://learn.microsoft.com/en-us/azure/cognitive-services/speech-service/language-support?tabs=tts) in case you want to change the default voice or give specific users different voices

## Chat commands
| Command | Example Usage | Description | Permissions
| - | - | - | - |
| !ttshelp | !ttshelp | Prints TTS version info and a link to this page in chat | Anyone |
| !ttsvoice | !ttsvoice en-US-AmberNeural | Changes default TTS voice | Broadcaster, Moderator |
| !ttsmute | !ttsmute CatseTheCat | Mutes a specific user from TTS | Broadcaster, Moderator |
| !ttsunmute | !ttsunmute CatseTheCat | Unmutes a specific user from TTS | Broadcaster, Moderator |
| !ttsnickname | !ttsnickname CatseTheCat catsy | Gives a TTS nickname to a specific user | Broadcaster, Moderator |
| !ttsuservoice | !ttsuservoice CatseTheCat en-US-AmberNeural | Gives a custom voice to a specific user | Broadcaster, Moderator |
