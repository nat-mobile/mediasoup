#define MS_CLASS "RTC::Room"
// #define MS_LOG_DEV

#include "RTC/Room.hpp"
#include "Logger.hpp"
#include "Settings.hpp"
#include "MediaSoupError.hpp"
#include "Utils.hpp"
#include <cmath> // std::lround()
#include <set>
#include <string>
#include <vector>
#include <iostream>

namespace VP9
{
    /////////////////////////////////////////////////
    
    struct VP9InterPictureDependency
    {
        
        /*
         T: The temporal layer ID of current frame.  In the case of non-
         flexible mode, if PID is mapped to a frame in a specified GOF,
         then the value of T MUST match the corresponding T value of the
         mapped frame in the GOF.
         */
        uint8_t temporalLayerId;
        
        /*
         U: Switching up point.  If this bit is set to 1 for the current
         frame with temporal layer ID equal to T, then "switch up" to a
         higher frame rate is possible as subsequent higher temporal
         layer frames will not depend on any frame before the current
         frame (in coding time) with temporal layer ID greater than T.
         */
        bool switchingPoint;
        
        /*
         P_DIFF:  The reference index (in 7 bits) specified as the relative
         PID from the current frame.  For example, when P_DIFF=3 on a
         packet containing the frame with PID 112 means that the frame
         refers back to the frame with PID 109.  This calculation is
         done modulo the size of the PID field, i.e., either 7 or 15
         bits.
         */
        std::vector<uint8_t> referenceIndexDiff;
        
        
        VP9InterPictureDependency();
        uint32_t GetSize();
        uint32_t Parse(uint8_t* data, uint32_t size);
        
        uint32_t Serialize(uint8_t *data,uint32_t size);
        void Dump();
        
    };
    struct VP9ScalabilityScructure
    {
        /*
         N_S:  N_S + 1 indicates the number of spatial layers present in the
         VP9 stream.
         */
        uint8_t numberSpatialLayers;
        /*
         Y: Each spatial layer's frame resolution present.  When set to one,
         the OPTIONAL WIDTH (2 octets) and HEIGHT (2 octets) MUST be
         present for each layer frame.  Otherwise, the resolution MUST NOT
         be present.
         */
        bool spatialLayerFrameResolutionPresent;
        /*
         G: GOF description present flag.
         */
        bool groupOfFramesDescriptionPresent;
        
        /*
         -: Bit reserved for future use.  MUST be set to zero and MUST be
         ignored by the receiver.
         NOTE: spatialLayerFrameResolutions.size()==numberSpatialLayers
         */
        std::vector<std::pair<uint16_t,uint16_t>> spatialLayerFrameResolutions;
        
        /*
         N_G:  N_G indicates the number of frames in a GOF.  If N_G is greater
         than 0, then the SS data allows the inter-picture dependency
         structure of the VP9 stream to be pre-declared, rather than
         indicating it on the fly with every packet.  If N_G is greater
         than 0, then for N_G pictures in the GOF, each frame's temporal
         layer ID (T), switch up point (U), and the R reference indices
         (P_DIFFs) are specified.
         
         The very first frame specified in the GOF MUST have T set to 0.
         
         G set to 0 or N_G set to 0 indicates that either there is only one
         temporal layer or no fixed inter-picture dependency information is
         present going forward in the bitstream.
         
         Note that for a given super frame, all layer frames follow the
         same inter-picture dependency structure.  However, the frame rate
         of each spatial layer can be different from each other and this
         can be controlled with the use of the D bit described above.  The
         specified dependency structure in the SS data MUST be for the
         highest frame rate layer.
         */
        std::vector<VP9InterPictureDependency> groupOfFramesDescription;
        
        VP9ScalabilityScructure();
        uint32_t GetSize();
        uint32_t Parse(uint8_t* data, uint32_t size);
        
        uint32_t Serialize(uint8_t *data,uint32_t size);
        void Dump();
    };
    
    struct VP9PayloadDescription
    {
        /*
         I: Picture ID (PID) present.  When set to one, the OPTIONAL PID MUST
         be present after the mandatory first octet and specified as below.
         Otherwise, PID MUST NOT be present.
         */
        bool pictureIdPresent;
        /*
         P: Inter-picture predicted layer frame.  When set to zero, the layer
         frame does not utilize inter-picture prediction.  In this case,
         up-switching to current spatial layer's frame is possible from
         directly lower spatial layer frame.  P SHOULD also be set to zero
         when encoding a layer synchronization frame in response to an LRR
         [I-D.ietf-avtext-lrr] message (see Section 5.4).  When P is set to
         zero, the T bit (described below) MUST also be set to 0 (if
         present).
         */
        bool interPicturePredictedLayerFrame;
        /*
         L: Layer indices present.  When set to one, the one or two octets
         following the mandatory first octet and the PID (if present) is as
         described by "Layer indices" below.  If the F bit (described
         below) is set to 1 (indicating flexible mode), then only one octet
         is present for the layer indices.  Otherwise if the F bit is set
         to 0 (indicating non-flexible mode), then two octets are present
         for the layer indices.
         */
        bool layerIndicesPresent;
        /*
         F: Flexible mode.  F set to one indicates flexible mode and if the P
         bit is also set to one, then the octets following the mandatory
         first octet, the PID, and layer indices (if present) are as
         described by "Reference indices" below.  This MUST only be set to
         1 if the I bit is also set to one; if the I bit is set to zero,
         then this MUST also be set to zero and ignored by receivers.  The
         value of this F bit CAN ONLY CHANGE on the very first packet of a
         key picture.  This is a packet with the P bit equal to zero, S or
         D bit (described below) equal to zero, and B bit (described below)
         equal to 1.
         */
        bool flexibleMode;
        /*
         B: Start of a layer frame.  MUST be set to 1 if the first payload
         octet of the RTP packet is the beginning of a new VP9 layer frame,
         and MUST NOT be 1 otherwise.  Note that this layer frame might not
         be the very first layer frame of a super frame.
         
         */
        bool startOfLayerFrame;
        /*
         E: End of a layer frame.  MUST be set to 1 for the final RTP packet
         of a VP9 layer frame, and 0 otherwise.  This enables a decoder to
         finish decoding the layer frame, where it otherwise may need to
         wait for the next packet to explicitly know that the layer frame
         is complete.  Note that, if spatial scalability is in use, more
         layer frames from the same super frame may follow; see the
         description of the M bit above.
         */
        bool endOfLayerFrame;
        /*
         V: Scalability structure (SS) data present.  When set to one, the
         OPTIONAL SS data MUST be present in the payload descriptor.
         Otherwise, the SS data MUST NOT be present.
         */
        bool scalabiltiyStructureDataPresent;
        /*
         -: Bit reserved for future use.  MUST be set to zero and MUST be
         ignored by the receiver.
         */
        
        //reserved
        
        /*
         Picture ID (PID):  Picture ID represented in 7 or 15 bits, depending
         on the M bit.  This is a running index of the pictures.  The field
         MUST be present if the I bit is equal to one.  If M is set to
         zero, 7 bits carry the PID; else if M is set to one, 15 bits carry
         the PID in network uint8_t order.  The sender may choose between a 7-
         or 15-bit index.  The PID SHOULD start on a random number, and
         MUST wrap after reaching the maximum ID.  The receiver MUST NOT
         assume that the number of bits in PID stay the same through the
         session.
         
         In the non-flexible mode (when the F bit is set to 0), this PID is
         used as an index to the GOF specified in the SS data bleow.  In
         this mode, the PID of the key frame corresponds to the very first
         specified frame in the GOF.  Then subsequent PIDs are mapped to
         subsequently specified frames in the GOF (modulo N_G, specified in
         the SS data below), respectively.
         */
        uint16_t pictureId;
        
        /*
         T: The temporal layer ID of current frame.  In the case of non-
         flexible mode, if PID is mapped to a frame in a specified GOF,
         then the value of T MUST match the corresponding T value of the
         mapped frame in the GOF.
         */
        uint8_t temporalLayerId;
        
        /*
         U: Switching up point.  If this bit is set to 1 for the current
         frame with temporal layer ID equal to T, then "switch up" to a
         higher frame rate is possible as subsequent higher temporal
         layer frames will not depend on any frame before the current
         frame (in coding time) with temporal layer ID greater than T.
         */
        bool switchingPoint;
        
        /*
         S: The spatial layer ID of current frame.  Note that frames with
         spatial layer S > 0 may be dependent on decoded spatial layer
         S-1 frame within the same super frame.
         */
        uint8_t spatialLayerId;
        /*
         D: Inter-layer dependency used.  MUST be set to one if current
         spatial layer S frame depends on spatial layer S-1 frame of the
         same super frame.  MUST only be set to zero if current spatial
         layer S frame does not depend on spatial layer S-1 frame of the
         same super frame.  For the base layer frame with S equal to 0,
         this D bit MUST be set to zero.
         */
        bool interlayerDependencyUsed;
        /*
         TL0PICIDX:  8 bits temporal layer zero index.  TL0PICIDX is only
         present in the non-flexible mode (F = 0).  This is a running
         index for the temporal base layer frames, i.e., the frames with
         T set to 0.  If T is larger than 0, TL0PICIDX indicates which
         temporal base layer frame the current frame depends on.
         TL0PICIDX MUST be incremented when T is equal to 0.  The index
         SHOULD start on a random number, and MUST restart at 0 after
         reaching the maximum number 255.
         */
        uint8_t temporalLayer0Index;
        
        /*
         P_DIFF:  The reference index (in 7 bits) specified as the relative
         PID from the current frame.  For example, when P_DIFF=3 on a
         packet containing the frame with PID 112 means that the frame
         refers back to the frame with PID 109.  This calculation is
         done modulo the size of the PID field, i.e., either 7 or 15
         bits.
         */
        std::vector<uint8_t> referenceIndexDiff;
        
        /*
         The scalability structure (SS) data describes the resolution of each
         layer frame within a super frame as well as the inter-picture
         dependencies for a group of frames (GOF).  If the VP9 payload
         descriptor's "V" bit is set, the SS data is present in the position
         indicated in Figure 2 and Figure 3.	 
         */
        VP9ScalabilityScructure scalabilityStructure;
        
        VP9PayloadDescription();
        uint32_t GetSize();
        uint32_t Parse(uint8_t* data, uint32_t size);
        
        uint32_t Serialize(uint8_t *data,uint32_t size);
        void Dump();
    };
    
    class VP9LayerSelector
    {
    public:
        static uint8_t MaxLayerId;
    public:
        VP9LayerSelector();
        VP9LayerSelector(uint8_t temporalLayerId,uint8_t spatialLayerId );
        void SelectTemporalLayer(uint8_t id);
        void SelectSpatialLayer(uint8_t id);
        
        bool Select(RTC::RtpPacket *packet,uint32_t &extSeqNum,bool &mark);
        
        uint8_t GetTemporalLayer() const	{ return temporalLayerId; }
        uint8_t GetSpatialLayer()	const	{ return spatialLayerId;  }
        
    private:
        uint8_t temporalLayerId;
        uint8_t spatialLayerId;
        uint8_t nextTemporalLayerId;
        uint8_t nextSpatialLayerId;
        uint32_t dropped;
    };
    
    /////////////////////////////////////////////////
    
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

}


namespace RTC
{
	/* Static. */

	static constexpr uint64_t AudioLevelsInterval{ 500 }; // In ms.

	/* Class variables. */

	RTC::RtpCapabilities Room::supportedRtpCapabilities;

	/* Class methods. */

	void Room::ClassInit()
	{
		MS_TRACE();

		// Parse all RTP capabilities.
		{
			// NOTE: These lines are auto-generated from data/supportedCapabilities.js.
			const std::string supportedRtpCapabilities =
			    R"({"codecs":[{"kind":"audio","name":"audio/opus","clockRate":48000,"numChannels":2,"rtcpFeedback":[]},{"kind":"audio","name":"audio/PCMU","clockRate":8000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/PCMA","clockRate":8000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/ISAC","clockRate":32000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/ISAC","clockRate":16000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/G722","clockRate":8000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/iLBC","clockRate":8000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/SILK","clockRate":24000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/SILK","clockRate":16000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/SILK","clockRate":12000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/SILK","clockRate":8000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/CN","clockRate":32000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/CN","clockRate":16000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/CN","clockRate":8000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/CN","clockRate":32000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/telephone-event","clockRate":48000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/telephone-event","clockRate":32000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/telephone-event","clockRate":16000,"rtcpFeedback":[]},{"kind":"audio","name":"audio/telephone-event","clockRate":8000,"rtcpFeedback":[]},{"kind":"video","name":"video/VP8","clockRate":90000,"rtcpFeedback":[{"type":"nack"},{"type":"nack","parameter":"pli"},{"type":"nack","parameter":"sli"},{"type":"nack","parameter":"rpsi"},{"type":"nack","parameter":"app"},{"type":"ccm","parameter":"fir"},{"type":"ack","parameter":"rpsi"},{"type":"ack","parameter":"app"},{"type":"goog-remb"}]},{"kind":"video","name":"video/VP9","clockRate":90000,"rtcpFeedback":[{"type":"nack"},{"type":"nack","parameter":"pli"},{"type":"nack","parameter":"sli"},{"type":"nack","parameter":"rpsi"},{"type":"nack","parameter":"app"},{"type":"ccm","parameter":"fir"},{"type":"ack","parameter":"rpsi"},{"type":"ack","parameter":"app"},{"type":"goog-remb"}]},{"kind":"video","name":"video/H264","clockRate":90000,"parameters":{"packetizationMode":0},"rtcpFeedback":[{"type":"nack"},{"type":"nack","parameter":"pli"},{"type":"nack","parameter":"sli"},{"type":"nack","parameter":"rpsi"},{"type":"nack","parameter":"app"},{"type":"ccm","parameter":"fir"},{"type":"ack","parameter":"rpsi"},{"type":"ack","parameter":"app"},{"type":"goog-remb"}]},{"kind":"video","name":"video/H264","clockRate":90000,"parameters":{"packetizationMode":1},"rtcpFeedback":[{"type":"nack"},{"type":"nack","parameter":"pli"},{"type":"nack","parameter":"sli"},{"type":"nack","parameter":"rpsi"},{"type":"nack","parameter":"app"},{"type":"ccm","parameter":"fir"},{"type":"ack","parameter":"rpsi"},{"type":"ack","parameter":"app"},{"type":"goog-remb"}]},{"kind":"video","name":"video/H265","clockRate":90000,"rtcpFeedback":[{"type":"nack"},{"type":"nack","parameter":"pli"},{"type":"nack","parameter":"sli"},{"type":"nack","parameter":"rpsi"},{"type":"nack","parameter":"app"},{"type":"ccm","parameter":"fir"},{"type":"ack","parameter":"rpsi"},{"type":"ack","parameter":"app"},{"type":"goog-remb"}]}],"headerExtensions":[{"kind":"audio","uri":"urn:ietf:params:rtp-hdrext:ssrc-audio-level","preferredId":1,"preferredEncrypt":false},{"kind":"video","uri":"urn:ietf:params:rtp-hdrext:toffset","preferredId":2,"preferredEncrypt":false},{"kind":"","uri":"http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time","preferredId":3,"preferredEncrypt":false},{"kind":"video","uri":"urn:3gpp:video-orientation","preferredId":4,"preferredEncrypt":false},{"kind":"","uri":"urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id","preferredId":5,"preferredEncrypt":false}],"fecMechanisms":[]})";

			Json::CharReaderBuilder builder;
			Json::Value settings = Json::nullValue;

			builder.strictMode(&settings);

			Json::CharReader* jsonReader = builder.newCharReader();
			Json::Value json;
			std::string jsonParseError;

			if (!jsonReader->parse(
			        supportedRtpCapabilities.c_str(),
			        supportedRtpCapabilities.c_str() + supportedRtpCapabilities.length(),
			        &json,
			        &jsonParseError))
			{
				delete jsonReader;

				MS_THROW_ERROR_STD(
				    "JSON parsing error in supported RTP capabilities: %s", jsonParseError.c_str());
			}
			else
			{
				delete jsonReader;
			}

			try
			{
				Room::supportedRtpCapabilities = RTC::RtpCapabilities(json, RTC::Scope::ROOM_CAPABILITY);
			}
			catch (const MediaSoupError& error)
			{
				MS_THROW_ERROR_STD("wrong supported RTP capabilities: %s", error.what());
			}
            
		}
	}

	/* Instance methods. */

	Room::Room(Listener* listener, Channel::Notifier* notifier, uint32_t roomId, Json::Value& data)
	    : roomId(roomId), listener(listener), notifier(notifier)
	{
		MS_TRACE();

		static const Json::StaticString JsonStringMediaCodecs{ "mediaCodecs" };

		// `mediaCodecs` is optional.
		if (data[JsonStringMediaCodecs].isArray())
		{
			auto& jsonMediaCodecs = data[JsonStringMediaCodecs];
			std::vector<RTC::RtpCodecParameters> mediaCodecs;

			for (auto& jsonMediaCodec : jsonMediaCodecs)
			{
				RTC::RtpCodecParameters mediaCodec(jsonMediaCodec, RTC::Scope::ROOM_CAPABILITY);

				// Ignore feature codecs.
				if (mediaCodec.mime.IsFeatureCodec())
					continue;

				// Check whether the given media codec is supported by mediasoup. If not
				// ignore it.
				for (auto& supportedMediaCodec : Room::supportedRtpCapabilities.codecs)
				{
					if (supportedMediaCodec.Matches(mediaCodec))
					{
						// Copy the RTCP feedback.
						mediaCodec.rtcpFeedback = supportedMediaCodec.rtcpFeedback;

						mediaCodecs.push_back(mediaCodec);

						break;
					}
				}
			}

			// Set room RTP capabilities.
			// NOTE: This may throw.
			SetCapabilities(mediaCodecs);
		}

		// Set the audio levels timer.
		this->audioLevelsTimer = new Timer(this);
        
        // create selector if needed
        if (Settings::configuration.vp9MinTemporial > 0 || Settings::configuration.vp9MinSpartial > 0)
        {
            m_vp9Selector = new VP9::VP9LayerSelector();
            m_vp9Selector->SelectTemporalLayer(Settings::configuration.vp9MinTemporial);
            m_vp9Selector->SelectSpatialLayer(Settings::configuration.vp9MinSpartial);
        }
	}

	Room::~Room()
	{
		MS_TRACE();
        delete m_vp9Selector;
	}

	void Room::Destroy()
	{
		MS_TRACE();

		static const Json::StaticString JsonStringClass{ "class" };

		Json::Value eventData(Json::objectValue);

		// Close all the Peers.
		// NOTE: Upon Peer closure the onPeerClosed() method is called which
		// removes it from the map, so this is the safe way to iterate the map
		// and remove elements.
		for (auto it = this->peers.begin(); it != this->peers.end();)
		{
			auto* peer = it->second;

			it = this->peers.erase(it);
			peer->Destroy();
		}

		// Close the audio level timer.
		this->audioLevelsTimer->Destroy();

		// Notify.
		eventData[JsonStringClass] = "Room";
		this->notifier->Emit(this->roomId, "close", eventData);

		// Notify the listener.
		this->listener->OnRoomClosed(this);

		delete this;
	}

	Json::Value Room::ToJson() const
	{
		MS_TRACE();

		static const Json::StaticString JsonStringRoomId{ "roomId" };
		static const Json::StaticString JsonStringCapabilities{ "capabilities" };
		static const Json::StaticString JsonStringPeers{ "peers" };
		static const Json::StaticString JsonStringMapRtpReceiverRtpSenders{ "mapRtpReceiverRtpSenders" };
		static const Json::StaticString JsonStringMapRtpSenderRtpReceiver{ "mapRtpSenderRtpReceiver" };
		static const Json::StaticString JsonStringAudioLevelsEventEnabled{ "audioLevelsEventEnabled" };

		Json::Value json(Json::objectValue);
		Json::Value jsonPeers(Json::arrayValue);
		Json::Value jsonMapRtpReceiverRtpSenders(Json::objectValue);
		Json::Value jsonMapRtpSenderRtpReceiver(Json::objectValue);

		// Add `roomId`.
		json[JsonStringRoomId] = Json::UInt{ this->roomId };

		// Add `capabilities`.
		json[JsonStringCapabilities] = this->capabilities.ToJson();

		// Add `peers`.
		for (auto& kv : this->peers)
		{
			auto* peer = kv.second;

			jsonPeers.append(peer->ToJson());
		}
		json[JsonStringPeers] = jsonPeers;

		// Add `mapRtpReceiverRtpSenders`.
		for (auto& kv : this->mapRtpReceiverRtpSenders)
		{
			auto rtpReceiver = kv.first;
			auto& rtpSenders = kv.second;
			Json::Value jsonRtpReceivers(Json::arrayValue);

			for (auto& rtpSender : rtpSenders)
			{
				jsonRtpReceivers.append(std::to_string(rtpSender->rtpSenderId));
			}

			jsonMapRtpReceiverRtpSenders[std::to_string(rtpReceiver->rtpReceiverId)] = jsonRtpReceivers;
		}
		json[JsonStringMapRtpReceiverRtpSenders] = jsonMapRtpReceiverRtpSenders;

		// Add `mapRtpSenderRtpReceiver`.
		for (auto& kv : this->mapRtpSenderRtpReceiver)
		{
			auto rtpSender   = kv.first;
			auto rtpReceiver = kv.second;

			jsonMapRtpSenderRtpReceiver[std::to_string(rtpSender->rtpSenderId)] =
			    std::to_string(rtpReceiver->rtpReceiverId);
		}
		json[JsonStringMapRtpSenderRtpReceiver] = jsonMapRtpSenderRtpReceiver;

		json[JsonStringAudioLevelsEventEnabled] = this->audioLevelsEventEnabled;

		return json;
	}

	void Room::HandleRequest(Channel::Request* request)
	{
		MS_TRACE();

		switch (request->methodId)
		{
			case Channel::Request::MethodId::ROOM_CLOSE:
			{
#ifdef MS_LOG_DEV
				uint32_t roomId = this->roomId;
#endif

				Destroy();

				MS_DEBUG_DEV("Room closed [roomId:%" PRIu32 "]", roomId);

				request->Accept();

				break;
			}

			case Channel::Request::MethodId::ROOM_DUMP:
			{
				auto json = ToJson();

				request->Accept(json);

				break;
			}

			case Channel::Request::MethodId::ROOM_CREATE_PEER:
			{
				static const Json::StaticString JsonStringPeerName{ "peerName" };

				RTC::Peer* peer;
				uint32_t peerId;
				std::string peerName;

				try
				{
					peer = GetPeerFromRequest(request, &peerId);
				}
				catch (const MediaSoupError& error)
				{
					request->Reject(error.what());

					return;
				}

				if (peer != nullptr)
				{
					request->Reject("Peer already exists");

					return;
				}

				if (!request->internal[JsonStringPeerName].isString())
				{
					request->Reject("Request has not string internal.peerName");

					return;
				}

				peerName = request->internal[JsonStringPeerName].asString();

				try
				{
					peer = new RTC::Peer(this, this->notifier, peerId, peerName);
				}
				catch (const MediaSoupError& error)
				{
					request->Reject(error.what());

					return;
				}

				// Store the new Peer.
				this->peers[peerId] = peer;

				MS_DEBUG_DEV("Peer created [peerId:%u, peerName:'%s']", peerId, peerName.c_str());

				request->Accept();

				break;
			}

			case Channel::Request::MethodId::ROOM_SET_AUDIO_LEVELS_EVENT:
			{
				static const Json::StaticString JsonStringEnabled{ "enabled" };

				if (!request->data[JsonStringEnabled].isBool())
				{
					request->Reject("Request has invalid data.enabled");

					return;
				}

				bool audioLevelsEventEnabled = request->data[JsonStringEnabled].asBool();

				if (audioLevelsEventEnabled == this->audioLevelsEventEnabled)
					return;

				// Clear map of audio levels.
				this->mapRtpReceiverAudioLevels.clear();

				// Start or stop audio levels periodic timer.
				if (audioLevelsEventEnabled)
					this->audioLevelsTimer->Start(AudioLevelsInterval, AudioLevelsInterval);
				else
					this->audioLevelsTimer->Stop();

				this->audioLevelsEventEnabled = audioLevelsEventEnabled;

				request->Accept();

				break;
			}

			case Channel::Request::MethodId::PEER_CLOSE:
			case Channel::Request::MethodId::PEER_DUMP:
			case Channel::Request::MethodId::PEER_SET_CAPABILITIES:
			case Channel::Request::MethodId::PEER_CREATE_TRANSPORT:
			case Channel::Request::MethodId::PEER_CREATE_RTP_RECEIVER:
			case Channel::Request::MethodId::TRANSPORT_CLOSE:
			case Channel::Request::MethodId::TRANSPORT_DUMP:
			case Channel::Request::MethodId::TRANSPORT_SET_REMOTE_DTLS_PARAMETERS:
			case Channel::Request::MethodId::TRANSPORT_SET_MAX_BITRATE:
			case Channel::Request::MethodId::TRANSPORT_CHANGE_UFRAG_PWD:
			case Channel::Request::MethodId::RTP_RECEIVER_CLOSE:
			case Channel::Request::MethodId::RTP_RECEIVER_DUMP:
			case Channel::Request::MethodId::RTP_RECEIVER_RECEIVE:
			case Channel::Request::MethodId::RTP_RECEIVER_SET_TRANSPORT:
			case Channel::Request::MethodId::RTP_RECEIVER_SET_RTP_RAW_EVENT:
			case Channel::Request::MethodId::RTP_RECEIVER_SET_RTP_OBJECT_EVENT:
			case Channel::Request::MethodId::RTP_SENDER_DUMP:
			case Channel::Request::MethodId::RTP_SENDER_SET_TRANSPORT:
			case Channel::Request::MethodId::RTP_SENDER_DISABLE:
			{
				RTC::Peer* peer;

				try
				{
					peer = GetPeerFromRequest(request);
				}
				catch (const MediaSoupError& error)
				{
					request->Reject(error.what());

					return;
				}

				if (peer == nullptr)
				{
					request->Reject("Peer does not exist");

					return;
				}

				peer->HandleRequest(request);

				break;
			}

			default:
			{
				MS_ERROR("unknown method");

				request->Reject("unknown method");
			}
		}
	}

	RTC::Peer* Room::GetPeerFromRequest(Channel::Request* request, uint32_t* peerId) const
	{
		MS_TRACE();

		static const Json::StaticString JsonStringPeerId{ "peerId" };

		auto jsonPeerId = request->internal[JsonStringPeerId];

		if (!jsonPeerId.isUInt())
			MS_THROW_ERROR("Request has not numeric .peerId field");

		// If given, fill peerId.
		if (peerId != nullptr)
			*peerId = jsonPeerId.asUInt();

		auto it = this->peers.find(jsonPeerId.asUInt());
		if (it != this->peers.end())
		{
			auto* peer = it->second;

			return peer;
		}

		return nullptr;
	}

	void Room::SetCapabilities(std::vector<RTC::RtpCodecParameters>& mediaCodecs)
	{
		MS_TRACE();

		// Set codecs.
		{
			// Available dynamic payload types.
			// clang-format off
			static const std::vector<uint8_t> DynamicPayloadTypes =
			{
				100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117,
				118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 96,  97,  98,  99,  77,  78,  79,  80,
				81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,  35,  36,  37,
				38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,
				56,  57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71
			};
			// clang-format om
			// Iterator for available dynamic payload types.
			auto dynamicPayloadTypeIt = DynamicPayloadTypes.begin();
			// Payload types used by the room.
			std::set<uint8_t> roomPayloadTypes;
			// Given media kinds.
			std::set<RTC::Media::Kind> roomKinds;

			// Set the given room codecs.
			for (auto& mediaCodec : mediaCodecs)
			{
				// The room has this kind.
				roomKinds.insert(mediaCodec.kind);

				// Set unique PT.

				// If the codec has PT and it's not already used, let it untouched.
				if (mediaCodec.hasPayloadType &&
				    roomPayloadTypes.find(mediaCodec.payloadType) == roomPayloadTypes.end())
				{
					;
				}
				// Otherwise assign an available PT.
				else
				{
					while (dynamicPayloadTypeIt != DynamicPayloadTypes.end())
					{
						uint8_t payloadType = *dynamicPayloadTypeIt;

						++dynamicPayloadTypeIt;

						if (roomPayloadTypes.find(payloadType) == roomPayloadTypes.end())
						{
							// Assign PT.
							mediaCodec.payloadType    = payloadType;
							mediaCodec.hasPayloadType = true;

							break;
						}
					}

					// If no one found, throw.
					if (!mediaCodec.hasPayloadType)
						MS_THROW_ERROR("no more available dynamic payload types for given media codecs");
				}

				// Store the selected PT.
				roomPayloadTypes.insert(mediaCodec.payloadType);

				// Append the codec to the room capabilities.
				this->capabilities.codecs.push_back(mediaCodec);
			}
		}

		// Add supported RTP header extensions.
		this->capabilities.headerExtensions = Room::supportedRtpCapabilities.headerExtensions;

		// Add supported FEC mechanisms.
		this->capabilities.fecMechanisms = Room::supportedRtpCapabilities.fecMechanisms;
	}

	inline void Room::AddRtpSenderForRtpReceiver(RTC::Peer* senderPeer, const RTC::RtpReceiver* rtpReceiver)
	{
		MS_TRACE();

		MS_ASSERT(senderPeer->HasCapabilities(), "sender peer has no capabilities");
		MS_ASSERT(rtpReceiver->GetParameters(), "rtpReceiver has no parameters");

		uint32_t rtpSenderId = Utils::Crypto::GetRandomUInt(10000000, 99999999);
		auto rtpSender = new RTC::RtpSender(senderPeer, this->notifier, rtpSenderId, rtpReceiver->kind);

		// Store into the maps.
		this->mapRtpReceiverRtpSenders[rtpReceiver].insert(rtpSender);
		this->mapRtpSenderRtpReceiver[rtpSender] = rtpReceiver;

		auto rtpParameters           = rtpReceiver->GetParameters();
		auto associatedRtpReceiverId = rtpReceiver->rtpReceiverId;

		// Attach the RtpSender to the peer.
		senderPeer->AddRtpSender(rtpSender, rtpParameters, associatedRtpReceiverId);
	}

	void Room::OnPeerClosed(const RTC::Peer* peer)
	{
		MS_TRACE();

		this->peers.erase(peer->peerId);
	}

	void Room::OnPeerCapabilities(RTC::Peer* peer, RTC::RtpCapabilities* capabilities)
	{
		MS_TRACE();

		// Remove those peer's capabilities not supported by the room.

		// Remove unsupported codecs and set the same PT.
		for (auto it = capabilities->codecs.begin(); it != capabilities->codecs.end();)
		{
			auto& peerCodecCapability = *it;
			auto it2                  = this->capabilities.codecs.begin();

			for (; it2 != this->capabilities.codecs.end(); ++it2)
			{
				auto& roomCodecCapability = *it2;

				if (roomCodecCapability.Matches(peerCodecCapability))
				{
					// Set the same payload type.
					peerCodecCapability.payloadType    = roomCodecCapability.payloadType;
					peerCodecCapability.hasPayloadType = true;

					// Remove the unsupported RTCP feedback from the given codec.
					peerCodecCapability.ReduceRtcpFeedback(roomCodecCapability.rtcpFeedback);

					break;
				}
			}

			if (it2 != this->capabilities.codecs.end())
				++it;
			else
				it = capabilities->codecs.erase(it);
		}

		// Remove unsupported header extensions.
		capabilities->ReduceHeaderExtensions(this->capabilities.headerExtensions);

		// Remove unsupported FEC mechanisms.
		capabilities->ReduceFecMechanisms(this->capabilities.fecMechanisms);

		// Get all the ready RtpReceivers of the others Peers in the Room and
		// create RtpSenders for this new Peer.
		for (auto& kv : this->peers)
		{
			auto* receiverPeer = kv.second;

			for (auto rtpReceiver : receiverPeer->GetRtpReceivers())
			{
				// Skip if the RtpReceiver has not parameters.
				if (rtpReceiver->GetParameters() == nullptr)
					continue;

				AddRtpSenderForRtpReceiver(peer, rtpReceiver);
			}
		}
	}

	void Room::OnPeerRtpReceiverParameters(const RTC::Peer* peer, RTC::RtpReceiver* rtpReceiver)
	{
		MS_TRACE();

		MS_ASSERT(rtpReceiver->GetParameters(), "rtpReceiver->GetParameters() returns no RtpParameters");

		// If this is a new RtpReceiver, iterate all the peers but this one and
		// create a RtpSender associated to this RtpReceiver for each Peer.
		if (this->mapRtpReceiverRtpSenders.find(rtpReceiver) == this->mapRtpReceiverRtpSenders.end())
		{
			// Ensure the entry will exist even with an empty array.
			this->mapRtpReceiverRtpSenders[rtpReceiver];

			for (auto& kv : this->peers)
			{
				auto* senderPeer = kv.second;

				// Skip receiver Peer.
				if (senderPeer == peer)
					continue;

				// Skip Peer with capabilities not set yet.
				if (!senderPeer->HasCapabilities())
					continue;

				AddRtpSenderForRtpReceiver(senderPeer, rtpReceiver);
			}
		}
		// If this is not a new RtpReceiver let's retrieve its updated parameters
		// and update with them all the associated RtpSenders.
		else
		{
			for (auto rtpSender : this->mapRtpReceiverRtpSenders[rtpReceiver])
			{
				// Provide the RtpSender with the parameters of the RtpReceiver.
				rtpSender->Send(rtpReceiver->GetParameters());
			}
		}
	}

	void Room::OnPeerRtpReceiverClosed(const RTC::Peer* /*peer*/, const RTC::RtpReceiver* rtpReceiver)
	{
		MS_TRACE();

		// If the RtpReceiver is in the map, iterate the map and close all the
		// RtpSenders associated to the closed RtpReceiver.
		if (this->mapRtpReceiverRtpSenders.find(rtpReceiver) != this->mapRtpReceiverRtpSenders.end())
		{
			// Make a copy of the set of RtpSenders given that Destroy() will be called
			// in all of them, producing onPeerRtpSenderClosed() that will remove it
			// from the map.
			auto rtpSenders = this->mapRtpReceiverRtpSenders[rtpReceiver];

			// Safely iterate the copy of the set.
			for (auto& rtpSender : rtpSenders)
			{
				rtpSender->Destroy();
			}

			// Finally delete the RtpReceiver entry in the map.
			this->mapRtpReceiverRtpSenders.erase(rtpReceiver);
		}
	}

	void Room::OnPeerRtpSenderClosed(const RTC::Peer* /*peer*/, RTC::RtpSender* rtpSender)
	{
		MS_TRACE();

		// Iterate all the receiver/senders map and remove the closed RtpSender from all the
		// RtpReceiver entries.
		for (auto& kv : this->mapRtpReceiverRtpSenders)
		{
			auto& rtpSenders = kv.second;

			rtpSenders.erase(rtpSender);
		}

		// Also remove the entry from the sender/receiver map.
		this->mapRtpSenderRtpReceiver.erase(rtpSender);
	}

	void Room::OnPeerRtpPacket(
	    const RTC::Peer* /*peer*/, RTC::RtpReceiver* rtpReceiver, RTC::RtpPacket* packet)
	{
		MS_TRACE();

		MS_ASSERT(
		    this->mapRtpReceiverRtpSenders.find(rtpReceiver) != this->mapRtpReceiverRtpSenders.end(),
		    "RtpReceiver not present in the map");

		auto& rtpSenders = this->mapRtpReceiverRtpSenders[rtpReceiver];
        
        // now we drop packets for all senders
        if (m_vp9Selector && packet->GetPayloadType() == 101)
        {
            {
                // debug info as error
                VP9::VP9PayloadDescription desc;
                if (desc.Parse(packet->GetPayload(), 1700))
                    std::cerr << " temporal " << (int)desc.temporalLayerId << " spatial " << (int)desc.spatialLayerId << std::endl;
            }
            // drop packets if needed
            uint32_t extSegNum;
            bool mark;
            if(m_vp9Selector->Select(packet, extSegNum, mark))
            {
                packet->SetSequenceNumber(extSegNum);
                uint16_t cicles = extSegNum >> 16;
                packet->SetExtendedSequenceNumber(((uint32_t)cicles) << 16 | packet->GetSequenceNumber());
                packet->SetMarker(mark);
                // Send the RtpPacket to all the RtpSenders associated to the RtpReceiver from which it was received.
                for (auto& rtpSender : rtpSenders)
                    rtpSender->SendRtpPacket(packet);
            }
            else
            {
                std::cerr << "packet was dropped" << std::endl;
            }
        }
        else
        {
            // Send the RtpPacket to all the RtpSenders associated to the RtpReceiver from which it was received.
            for (auto& rtpSender : rtpSenders)
                rtpSender->SendRtpPacket(packet);
        }

		// Update audio levels.
		if (this->audioLevelsEventEnabled)
		{
			uint8_t volume;
			bool voice;

			if (packet->ReadAudioLevel(&volume, &voice))
			{
				int8_t dBov = volume * -1;

				this->mapRtpReceiverAudioLevels[rtpReceiver].push_back(dBov);
			}
		}
	}

	void Room::OnPeerRtcpReceiverReport(
	    const RTC::Peer* /*peer*/, RTC::RtpSender* rtpSender, RTC::RTCP::ReceiverReport* report)
	{
		MS_TRACE();

		MS_ASSERT(
		    this->mapRtpSenderRtpReceiver.find(rtpSender) != this->mapRtpSenderRtpReceiver.end(),
		    "RtpSender not present in the map");

		rtpSender->ReceiveRtcpReceiverReport(report);
	}

	void Room::OnPeerRtcpFeedback(
	    const RTC::Peer* /*peer*/, RTC::RtpSender* rtpSender, RTC::RTCP::FeedbackPsPacket* packet)
	{
		MS_TRACE();

		MS_ASSERT(
		    this->mapRtpSenderRtpReceiver.find(rtpSender) != this->mapRtpSenderRtpReceiver.end(),
		    "RtpSender not present in the map");

		auto& rtpReceiver = this->mapRtpSenderRtpReceiver[rtpSender];

		rtpReceiver->ReceiveRtcpFeedback(packet);
	}

	void Room::OnPeerRtcpFeedback(
	    const RTC::Peer* /*peer*/, RTC::RtpSender* rtpSender, RTC::RTCP::FeedbackRtpPacket* packet)
	{
		MS_TRACE();

		MS_ASSERT(
		    this->mapRtpSenderRtpReceiver.find(rtpSender) != this->mapRtpSenderRtpReceiver.end(),
		    "RtpSender not present in the map");

		auto& rtpReceiver = this->mapRtpSenderRtpReceiver[rtpSender];

		rtpReceiver->ReceiveRtcpFeedback(packet);
	}

	void Room::OnPeerRtcpSenderReport(
	    const RTC::Peer* /*peer*/, RTC::RtpReceiver* rtpReceiver, RTC::RTCP::SenderReport* report)
	{
		MS_TRACE();

		// RtpReceiver needs the sender report in order to generate it's receiver report.
		rtpReceiver->ReceiveRtcpSenderReport(report);

		MS_ASSERT(
		    this->mapRtpReceiverRtpSenders.find(rtpReceiver) != this->mapRtpReceiverRtpSenders.end(),
		    "RtpReceiver not present in the map");
	}

	void Room::OnFullFrameRequired(RTC::Peer* /*peer*/, RTC::RtpSender* rtpSender)
	{
		MS_TRACE();

		MS_ASSERT(
		    this->mapRtpSenderRtpReceiver.find(rtpSender) != this->mapRtpSenderRtpReceiver.end(),
		    "RtpSender not present in the map");

		auto& rtpReceiver = this->mapRtpSenderRtpReceiver[rtpSender];

		rtpReceiver->RequestFullFrame();
	}

	inline void Room::OnTimer(Timer* timer)
	{
		MS_TRACE();

		static const Json::StaticString JsonStringClass{ "class" };
		static const Json::StaticString JsonStringEntries{ "entries" };

		// Audio levels timer.
		if (timer == this->audioLevelsTimer)
		{
			std::unordered_map<RTC::RtpReceiver*, int8_t> mapRtpReceiverAudioLevel;

			for (auto& kv : this->mapRtpReceiverAudioLevels)
			{
				auto rtpReceiver = kv.first;
				auto& dBovs = kv.second;
				int8_t avgdBov{ -127 };

				if (!dBovs.empty())
				{
					int16_t sumdBovs{ 0 };

					for (auto& dBov : dBovs)
					{
						sumdBovs += dBov;
					}

					avgdBov = static_cast<int8_t>(
						std::lround(sumdBovs / static_cast<int16_t>(dBovs.size())));
				}

				mapRtpReceiverAudioLevel[rtpReceiver] = avgdBov;
			}

			// Clear map.
			this->mapRtpReceiverAudioLevels.clear();

			// Emit event.
			Json::Value eventData(Json::objectValue);

			eventData[JsonStringClass] = "Room";
			eventData[JsonStringEntries] = Json::arrayValue;

			for (auto& kv : mapRtpReceiverAudioLevel)
			{
				auto& rtpReceiverId = kv.first->rtpReceiverId;
				auto& audioLevel = kv.second;
				Json::Value entry(Json::arrayValue);

				entry.append(Json::UInt{ rtpReceiverId });
				entry.append(Json::Int{ audioLevel });

				eventData[JsonStringEntries].append(entry);
			}

			this->notifier->Emit(this->roomId, "audiolevels", eventData);
		}
	}
} // namespace RTC
