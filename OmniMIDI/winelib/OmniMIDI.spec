@ stdcall modMessage(long long long long long) proxy_modMessage
@ stdcall IsKDMAPIAvailable() proxy_IsKDMAPIAvailable
@ stdcall ReturnKDMAPIVer() proxy_ReturnKDMAPIVer
@ stdcall InitializeKDMAPIStream() proxy_InitializeKDMAPIStream
@ stdcall TerminateKDMAPIStream() proxy_TerminateKDMAPIStream
@ stdcall ResetKDMAPIStream() proxy_ResetKDMAPIStream
@ stdcall SendDirectData(long) proxy_SendDirectData
@ stdcall SendDirectDataNoBuf(long) proxy_SendDirectData
@ stdcall SendDirectLongData(ptr long) proxy_SendDirectLongData
@ stdcall SendDirectLongDataNoBuf(ptr long) proxy_SendDirectLongData
@ stdcall SendCustomEvent(long long long) proxy_SendCustomEvent
@ stdcall DriverSettings(long long ptr long) proxy_DriverSettings
@ stdcall LoadCustomSoundFontsList(str) proxy_LoadCustomSoundFontsList
@ stdcall GetRenderingTime() proxy_GetRenderingTime
@ stdcall GetVoiceCount() proxy_GetVoiceCount