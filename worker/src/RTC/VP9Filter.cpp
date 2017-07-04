#include "RTC/VP9Filter.hpp"
#include "handles/Timer.hpp"

namespace VP9
{
    
    uint32_t get2(const uint8_t *data,size_t i) { return (uint32_t)(data[i+1]) | ((uint32_t)(data[i]))<<8; }
    void set2(uint8_t *data,size_t i,uint32_t val)
    {
        data[i+1] = (uint8_t)(val);
        data[i]   = (uint8_t)(val>>8);
    }
    
    /*
     +-+-+-+-+-+-+-+-+
     N_G: |  T  |U| R |-|-| (OPTIONAL)
     +-+-+-+-+-+-+-+-+              -\
     |    P_DIFF     | (OPTIONAL)    . - R times
     +-+-+-+-+-+-+-+-+              -/
     */
    
    VP9InterPictureDependency::VP9InterPictureDependency()
    {
        temporalLayerId = 0;
        switchingPoint = false;
    }
    
    uint32_t VP9InterPictureDependency::GetSize()
    {
        return 1 + referenceIndexDiff.size();
    }
    
    uint32_t VP9InterPictureDependency::Parse(uint8_t* data, uint32_t size)
    {
        //Get values
        temporalLayerId = data[0] >> 5;
        switchingPoint  = data[0] & 0x10;
        //Number of pdifs
        uint8_t pdifs = data[0] >> 2 & 0x03;
        //Get each one
        for (uint8_t j=0;j<pdifs;++j)
            //Get it
            referenceIndexDiff.push_back(data[j+1]);
        //Return length
        return 1+pdifs;
    }
    
    uint32_t VP9InterPictureDependency::Serialize(uint8_t *data,uint32_t size)
    {
        //Check size
        if (size<GetSize())
            //Error
            return 0;
        
        //Doubel check size
        if (referenceIndexDiff.size()>3)
            //Error
            return 0;
        
        //Set number of frames in gof
        data[0] = temporalLayerId;
        data[0] = data[0]<<1 | switchingPoint;
        data[0] = data[0]<<2 | referenceIndexDiff.size() ;
        //Reserved
        data[0] = data[0] << 2;
        
        //Len
        uint32_t len = 1;
        
        //Get each one
        for (auto it = referenceIndexDiff.begin(); it!=referenceIndexDiff.end(); ++it)
        {
            //Get it
            data[len] = *it;
            //Inc
            len ++;
        }
        
        //Return length
        return len;
    }
    
    /*
     +-+-+-+-+-+-+-+-+
     V:   | N_S |Y|G|-|-|-|
     +-+-+-+-+-+-+-+-+              -\
     Y:   |     WIDTH     | (OPTIONAL)    .
     +               +               .
     |               | (OPTIONAL)    .
     +-+-+-+-+-+-+-+-+               . - N_S + 1 times
     |     HEIGHT    | (OPTIONAL)    .
     +               +               .
     |               | (OPTIONAL)    .
     +-+-+-+-+-+-+-+-+              -/
     G:   |      N_G      | (OPTIONAL)
     +-+-+-+-+-+-+-+-+
     |...............|  N_G * VP9InterPictureDependency
     |...............|
     +-+-+-+-+-+-+-+-+
     */
    
    VP9ScalabilityScructure::VP9ScalabilityScructure()
    {
        numberSpatialLayers = 0;
        spatialLayerFrameResolutionPresent = false;
        groupOfFramesDescriptionPresent = false;
    }
    
    uint32_t VP9ScalabilityScructure::GetSize()
    {
        //Heder
        uint32_t len =  1;
        
        //If we have spatial resolutions
        if (spatialLayerFrameResolutionPresent)
            //Increase length
            len += spatialLayerFrameResolutions.size()*4;
        
        //Is gof description present
        if (groupOfFramesDescriptionPresent)
        {
            //Inc len
            len ++;
            //Fore each  oen
            for (auto it = groupOfFramesDescription.begin(); it!=groupOfFramesDescription.end(); ++it)
                //Inc length
                len += it->GetSize();
        }
        
        //REturn length
        return len;
    }
    
    uint32_t VP9ScalabilityScructure::Parse(uint8_t* data, uint32_t size)
    {
        //Check kength
        if (size<1)
            //Error
            return 0;
        
        //Parse header
        numberSpatialLayers			= (data[0] >> 5) + 1;
        spatialLayerFrameResolutionPresent	= data[0] & 0x10;
        groupOfFramesDescriptionPresent		= data[0] & 0x0F;
        
        //Heder
        uint32_t len =  1;
        
        //If we have spatial resolutions
        if (spatialLayerFrameResolutionPresent)
        {
            //Endusre lenght
            if (len+numberSpatialLayers*4>size)
                //Error
                return 0;
            
            //For each spatial layer
            for (uint8_t i=0;i<numberSpatialLayers;++i)
            {
                //Get dimensions
                uint16_t width = get2(data,len);
                uint16_t height = get2(data,len+2);
                //Append
                spatialLayerFrameResolutions.push_back(std::make_pair(width,height));
                //Inc length
                len += 4;
                
            }
        }
        
        //Is gof description present
        if (groupOfFramesDescriptionPresent)
        {
            //Get number of frames in group
            uint8_t n = data[len];
            //Inc len
            len ++;
            //Fore each  one
            for (uint8_t i=0; i<n;++i)
            {
                //Dependency
                VP9InterPictureDependency dependency;
                //Parse it
                uint32_t l = dependency.Parse(data+len,size-len);
                //If failed
                if (!l)
                    //Error
                    return 0;
                //Append
                groupOfFramesDescription.push_back(dependency);
                //Inc lenght
                len += l;
            }
        }
        
        //REturn length
        return len;
    }
    
    uint32_t VP9ScalabilityScructure::Serialize(uint8_t *data,uint32_t size)
    {
        //Check size
        if (size<GetSize())
            //Error
            return 0;
        
        //Serialize header
        data[0] = numberSpatialLayers-1;
        data[0] = data[0] << 1 | spatialLayerFrameResolutionPresent;
        data[0] = data[0] << 1 | groupOfFramesDescriptionPresent;
        //Reserved
        data[0] = data[0] << 3;
        //Heder
        uint32_t len =  1;
        
        //If we have spatial resolutions
        if (spatialLayerFrameResolutionPresent)
        {
            //For each spatial layer
            for (auto it = spatialLayerFrameResolutions.begin(); it!= spatialLayerFrameResolutions.end(); ++it)
            {
                //Set dimensions
                set2(data,len,it->first);
                set2(data,len+2,it->second);
                //Inc length
                len += 4;
            }
        }
        
        //Is gof description present
        if (groupOfFramesDescriptionPresent)
        {
            //Set number of grames
            data[len] = groupOfFramesDescription.size();
            //Inc len
            len ++;
            //Fore each  one
            for (auto it = groupOfFramesDescription.begin(); it!=groupOfFramesDescription.end(); ++it)
            {
                //Serialize it
                uint32_t l = it->Serialize(data+len,size-len);
                //If failed
                if (!l)
                    //Error
                    return 0;
                //Inc lenght
                len += l;
            }
        }
        
        //REturn length
        return len;
    }
    
    /*
     In flexible mode (with the F bit below set to 1), The first octets
     after the RTP header are the VP9 payload descriptor, with the
     following structure.
     
     0 1 2 3 4 5 6 7
     +-+-+-+-+-+-+-+-+
     |I|P|L|F|B|E|V|-| (REQUIRED)
     +-+-+-+-+-+-+-+-+
     I:   |M| PICTURE ID  | (REQUIRED)
     +-+-+-+-+-+-+-+-+
     M:   | EXTENDED PID  | (RECOMMENDED)
     +-+-+-+-+-+-+-+-+
     L:   |  T  |U|  S  |D| (CONDITIONALLY RECOMMENDED)
     +-+-+-+-+-+-+-+-+                             -\
     P,F: | P_DIFF      |N| (CONDITIONALLY REQUIRED)    - up to 3 times
     +-+-+-+-+-+-+-+-+                             -/
     V:   | SS            |
     | ..            |
     +-+-+-+-+-+-+-+-+
     
     In non-flexible mode (with the F bit below set to 0), The first
     octets after the RTP header are the VP9 payload descriptor, with the
     following structure.
     
     0 1 2 3 4 5 6 7
     +-+-+-+-+-+-+-+-+
     |I|P|L|F|B|E|V|-| (REQUIRED)
     +-+-+-+-+-+-+-+-+
     I:   |M| PICTURE ID  | (RECOMMENDED)
     +-+-+-+-+-+-+-+-+
     M:   | EXTENDED PID  | (RECOMMENDED)
     +-+-+-+-+-+-+-+-+
     L:   |  T  |U|  S  |D| (CONDITIONALLY RECOMMENDED)
     +-+-+-+-+-+-+-+-+
     |   TL0PICIDX   | (CONDITIONALLY REQUIRED)
     +-+-+-+-+-+-+-+-+
     V:   | SS            |
     | ..            |
     +-+-+-+-+-+-+-+-+
     
     */
    
    VP9PayloadDescription::VP9PayloadDescription()
    {
        pictureIdPresent = 0;
        interPicturePredictedLayerFrame = 0;
        layerIndicesPresent = 0;
        flexibleMode = 0;
        startOfLayerFrame = 0;
        endOfLayerFrame = 0;
        scalabiltiyStructureDataPresent = 0;
        pictureId = 0;
        temporalLayerId = 0;
        switchingPoint = 0;
        spatialLayerId = 0;
        interlayerDependencyUsed = 0;
        temporalLayer0Index = 0;
    }
    
    
    uint32_t VP9PayloadDescription::GetSize()
    {
        //Heder
        uint32_t len =  1;
        
        if (pictureIdPresent)
            //2 or 1 uint8_t?
            len += pictureId>0x7F ? 2 : 1;
        
        //if we have layer index
        if (layerIndicesPresent)
        {
            //Inc
            len ++;
            //Non-flexible
            if (!flexibleMode)
                //TL0PICIDX
                len += 1;
        }
        
        if (flexibleMode && pictureIdPresent)
            //n P diffs
            len += referenceIndexDiff.size();
        
        if (scalabiltiyStructureDataPresent)
        {
            //Get SS size
            uint32_t l = scalabilityStructure.GetSize();
            //Check
            if (!l)
                //Return error
                return 0;
            //Inc
            len += l;
        }
        
        //REturn length
        return len;
    }
    
    uint32_t VP9PayloadDescription::Parse(uint8_t* data, uint32_t size)
    {
        //Check kength
        if (size<1)
            //Error
            return 0;
        
        if (!data)
            //Error
            return 0;
        
        //Parse header
        pictureIdPresent		= data[0] >> 7 & 0x01;
        interPicturePredictedLayerFrame = data[0] >> 6 & 0x01;
        layerIndicesPresent		= data[0] >> 5 & 0x01;
        flexibleMode			= data[0] >> 4 & 0x01;
        startOfLayerFrame		= data[0] >> 3 & 0x01;
        endOfLayerFrame			= data[0] >> 2 & 0x01;
        scalabiltiyStructureDataPresent = data[0] >> 1 & 0x01;
        
        //Heder
        uint32_t len =  1;
        
        //Check pictrure id
        if (pictureIdPresent)
        {
            //If marker bit present
            if (data[len]>>7)
            {
                //Check kength
                if (size<len+2)
                    //Error
                    return 0;
                //15 bits
                pictureId = get2(data,len) & 0x7FFF;
                //Inc len
                len += 2;
            } else {
                //Check kength
                if (size<len+1)
                    //Error
                    return 0;
                //1 bit
                pictureId = data[len];
                //Inc len
                len ++;
            }
            
        }
        
        //if we have layer index
        if (layerIndicesPresent)
        {
            //Check kength
            if (size<len+1)
                //Error
                return 0;
            //Get indices
            temporalLayerId	 	 = data[len] >> 5;
            switchingPoint		 = data[len] & 0x10;
            spatialLayerId		 = (data[len] >> 1 ) & 0x07;
            interlayerDependencyUsed = data[len] & 0x01;
            //Inc
            len ++;
            
            //Only on nonf flexible mode
            if (!flexibleMode)
            {
                //Check kength
                if (size<len+1)
                    //Error
                    return 0;
                //TL0PICIDX
                temporalLayer0Index = data[len];
                //Inc length
                len += 1;
            }
        }
        
        if (flexibleMode && pictureIdPresent)
        {
            //Check kength
            if (size<len+1)
                //Error
                return 0;
            //Check last diff mark
            while (data[len] & 0x01)
            {
                //Add ref index
                referenceIndexDiff.push_back(data[len]>>1);
                //Inc len
                len ++;
            }
        }
        
        if (scalabiltiyStructureDataPresent)
        {
            //Get SS size
            uint32_t l = scalabilityStructure.Parse(data+len,size-len);
            //Check
            if (!l)
                //Return error
                return 0;
            //Inc
            len += l;
        }
        
        //REturn length
        return len;
    }
    
    uint32_t VP9PayloadDescription::Serialize(uint8_t *data,uint32_t size)
    {
        //Check kength
        if (size<GetSize())
            //Error
            return 0;
        
        //Parse header
        data[0] = pictureIdPresent;
        data[0] = data[0] << 1 | interPicturePredictedLayerFrame;
        data[0] = data[0] << 1 |layerIndicesPresent;
        data[0] = data[0] << 1 |flexibleMode;
        data[0] = data[0] << 1 |startOfLayerFrame;
        data[0] = data[0] << 1 |endOfLayerFrame;
        data[0] = data[0] << 1 |scalabiltiyStructureDataPresent;
        //reserved
        data[0] = data[0] << 1;
        
        //Heder
        uint32_t len =  1;
        
        //Check pictrure id
        if (pictureIdPresent)
        {
            //Check size
            if (pictureId > 0x7F)
            {
                //1 bit mark + 15 bits
                set2(data,len,pictureId);
                //Set martk
                data[len] = data[len] | 0x80000;
                //Inc len
                len += 2;
            } else {
                //1 bit
                data[len] = pictureId;
                //Inc len
                len ++;
            }
            
        }
        
        //if we have layer index
        if (layerIndicesPresent)
        {
            //Get indices
            data[len] = temporalLayerId;
            data[len] = data[len] << 1 | switchingPoint;
            data[len] = data[len] << 3 | (spatialLayerId & 0x03);
            data[len] = data[len] << 1 | interlayerDependencyUsed;
            //Inc
            len ++;
            //If not on flexible mode
            if (!flexibleMode)
            {
                //TL0PICIDX
                data[len] = temporalLayer0Index;
                //Inc length
                len += 1;
            }
        }
        
        if (flexibleMode && pictureIdPresent)
        {
            //Serialize picture refid
            for (auto it=referenceIndexDiff.begin(); it!=referenceIndexDiff.end(); ++it)
            {
                //Add ref index
                data[len] = (*it) << 1;
                //if  not last
                if (it!=referenceIndexDiff.end())
                    //Add marker
                    data[len] = data[len] | 0x01;
                //Inc len
                len ++;
            }
        }
        
        
        if (scalabiltiyStructureDataPresent)
        {
            //Get SS size
            uint32_t l = scalabilityStructure.Serialize(data+len,size-len);
            //Check
            if (!l)
                //Return error
                return 0;
            //Inc
            len += l;
        }
        
        //REturn length
        return len;
    }
    
    
    uint8_t VP9LayerSelector::MaxLayerId = 0xFF;
    
    VP9LayerSelector::VP9LayerSelector()
    {
        temporalLayerId = 0;
        spatialLayerId  = 0;
        nextTemporalLayerId = MaxLayerId;
        nextSpatialLayerId = MaxLayerId;
        dropped = 0;
    }
    
    VP9LayerSelector::VP9LayerSelector(uint8_t temporalLayerId,uint8_t spatialLayerId )
    {
        this->temporalLayerId = temporalLayerId;
        this->spatialLayerId  = spatialLayerId;
        nextTemporalLayerId = temporalLayerId;
        nextSpatialLayerId = spatialLayerId;
        dropped = 0;
    }
    
    void VP9LayerSelector::SelectTemporalLayer(uint8_t id)
    {
        //Set next
        nextTemporalLayerId = id;
    }
    
    void VP9LayerSelector::SelectSpatialLayer(uint8_t id)
    {
        //Set next
        nextSpatialLayerId = id;
    }
    
    bool VP9LayerSelector::Select(RTC::RtpPacket *packet,uint32_t &extSeqNum,bool &mark)
    {
        VP9PayloadDescription desc;
        
        //Parse VP9 payload description
        if (!desc.Parse(packet->GetPayload(), 1700))
            //Error
            return 0;
        
        //if (desc.startOfLayerFrame)
        //	UltraDebug("-VP9LayerSelector::Select() | #%d T%dS%d P=%d D=%d S=%d %s\n", desc.pictureId-42,desc.temporalLayerId,desc.spatialLayerId,desc.interPicturePredictedLayerFrame,desc.interlayerDependencyUsed,desc.switchingPoint
        //		,desc.interPicturePredictedLayerFrame==0 && desc.spatialLayerId==1 ? "<----------------------":"");
        
        //Store current temporal id
        uint8_t currentTemporalLayerId = temporalLayerId;
        
        //Check if we need to upscale temporally
        if (nextTemporalLayerId>temporalLayerId)
        {
            //Check if we can upscale and it is the start of the layer and it is a valid layer
            if (desc.switchingPoint && desc.startOfLayerFrame && desc.temporalLayerId<=nextTemporalLayerId)
            {
                //Update current layer
                temporalLayerId = desc.temporalLayerId;
            }
            //Check if we need to downscale
        } else if (nextTemporalLayerId<temporalLayerId) {
            //We can only downscale on the end of a layer to set the market bit
            if (desc.endOfLayerFrame)
            {
                //Update to target layer
                temporalLayerId = nextTemporalLayerId;
            }
        }
        
        //If it is from the current layer
        if (currentTemporalLayerId < desc.temporalLayerId)
        {
            //UltraDebug("-VP9LayerSelector::Select() | dropping packet based on temporalLayerId [us:%d,desc:%d,mark:%d]\n",temporalLayerId,desc.temporalLayerId,packet->GetMark());
            //INcrease dropped
            dropped++;
            //Drop it
            return false;
        }
        
        //Get current spatial layer
        uint8_t currentSpatialLayerId = spatialLayerId;
        
        //Check if we need to upscale spatially
        if (nextSpatialLayerId>spatialLayerId)
        {
            /*
             Inter-picture predicted layer frame.  When set to zero, the layer
             frame does not utilize inter-picture prediction.  In this case,
             up-switching to current spatial layer's frame is possible from
             directly lower spatial layer frame.  P SHOULD also be set to zero
             when encoding a layer synchronization frame in response to an LRR
             */
            //Check if we can upscale and it is the start of the layer and it is a valid layer
            if (desc.interPicturePredictedLayerFrame==0 && desc.startOfLayerFrame && desc.spatialLayerId==spatialLayerId+1)
            {
                //Update current layer
                spatialLayerId = desc.spatialLayerId;
            }
            //Ceck if we need to downscale
        } else if (nextSpatialLayerId<spatialLayerId) {
            //We can only downscale on the end of a layer to set the market bit
            if (desc.endOfLayerFrame)
            {
                //Update to target layer
                spatialLayerId = nextSpatialLayerId;
            }
        }
        
        //If it is from the current layer
        if (currentSpatialLayerId < desc.spatialLayerId)
        {
            //UltraDebug("-VP9LayerSelector::Select() | dropping packet based on spatialLayerId [us:%d,desc:%d,mark:%d]\n",spatialLayerId,desc.spatialLayerId,packet->GetMark());
            //INcrease dropped
            dropped++;
            //Drop it
            return false;
        }
        
        
        //Calculate new packet number removing the dropped pacekts by the selection layer
        extSeqNum = packet->GetExtendedSequenceNumber()-dropped;
        
        //RTP mark is set for the last frame layer of the selected layer
        mark = packet->HasMarker() || (desc.endOfLayerFrame && spatialLayerId==desc.spatialLayerId);
        
        //UltraDebug("-VP9LayerSelector::Select() | Accepting packet [extSegNum:%u,mark:%d,tid:%d,sid:%d,dropped:%d,orig:%u]\n",extSeqNum,mark,desc.temporalLayerId,desc.spatialLayerId,dropped,packet->GetExtSeqNum());
        //Select
        return true;
        
    }

    const uint64_t dropTimerInterval = 6000;
    const uint64_t keepTimerInterval = 2000;
    
    
    VP9AudioLevelSelector::VP9AudioLevelSelector():dropped(0),lastFilteredPacketNumber(0)
    {
        this->dropTimer = new Timer(this);
        this->keepTimer = new Timer(this);
    };
    
    VP9AudioLevelSelector::~VP9AudioLevelSelector()
    {
        this->dropTimer->Destroy();
        this->keepTimer->Destroy();
    }
    
    bool VP9AudioLevelSelector::Select(RTC::RtpPacket *packet, bool forceSelect, uint32_t &extSeqNum,bool &mark)
    {
        bool selected = true;
        if (forceSelect)
        {
            // no need to filter packets - stop timers and update packet number
            dropTimer->Stop();
            keepTimer->Stop();
        }
        else
        {
            // filter packet
            VP9PayloadDescription desc;
            //Parse VP9 payload description
            if (!desc.Parse(packet->GetPayload(), 1700))
                //Error
                return 0;
            // check if we need to filter current packet
            if(lastFilteredPacketNumber + 1 == packet->GetExtendedSequenceNumber())
            {
                // we checked prev packet and also need to check this one
                if (dropTimer->IsActive())
                {
                    // drop packet
                    dropped++;
                    selected = false;
                }
                // increase counters
                lastFilteredPacketNumber = packet->GetExtendedSequenceNumber();
            }
            else
            {
                dropTimer->Stop();
                keepTimer->Stop();
                //we dont chek prev packet wait end of frame
                if (desc.endOfLayerFrame)
                {
                    lastFilteredPacketNumber = packet->GetExtendedSequenceNumber();
                    dropTimer->Start(dropTimerInterval);
                }
            }
        }
        // update counters
        if(selected)
        {
            //Calculate new packet number removing the dropped pacekts by the selection layer
            extSeqNum = packet->GetExtendedSequenceNumber() - dropped;
            //RTP mark is set for the last frame layer of the selected layer
            mark = packet->HasMarker();
        }
        return selected;
    }
    
    inline void VP9AudioLevelSelector::OnTimer(Timer* timer)
    {
        if (timer == dropTimer)
        {
            dropTimer->Stop();
            keepTimer->Start(keepTimerInterval);
        }
        else if (timer == keepTimer)
        {
            keepTimer->Stop();
            dropTimer->Start(dropTimerInterval);
        }
    }
    
    
}
