# Porting this ManageabilityTransportSmbusLib



##  Step:
1.  HelperAcquireManageabilityTransport: init mToken
      -> Check spec name
      -> AcquireTransportSession ()
      -> Check spec name again?
2.  Setup Hardware info
      -> TDB
3.  Init transport interface
      -> Function.Version1_0->TransportInit ()




AcquireTransportSession () -> to get token

GetTransportCapability () -> unsupported


ReleaseTransportSession () -> Release (clean up !!!)


support this MANAGEABILITY_TRANSPORT_FUNCTION_V1_0