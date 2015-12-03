#!/usr/bin/env python

from struct import *
import ROOT

def read_file_header( f ):
    #apparently the time is time_t and *NOT* uint32 like the manual says
    b = f.read(8)
    timestamp = unpack('q',b)[0]

    b= f.read(4)
    runtype = unpack('i',b)[0]

    b = f.read(4)
    headlength = unpack('I',b)[0]

    b = f.read(headlength)
    fullinfo = unpack( str(headlength) + 's', b)[0]
    try:
        info = fullinfo[:fullinfo.index('\x00')]
    except:
        print "Failed to truncate fullinfo"
        info = fullinfo

    b = f.read(256*8)
    pedestal = unpack( '256d', b)

    b = f.read(256*8)
    noise = unpack( '256d', b)

    return timestamp, runtype, info, pedestal, noise

def read_block( f ):
    b = f.read(4)
    fullblocktype = unpack('I',b)[0]

    #check that it is as expected
    if (fullblocktype & 0xffff0000) != 0xcafe0000:
        print "Bad format for start of datablock header"
        blocktype = 0
    else:
        blocktype = fullblocktype & 0x0000ffff
    
    b = f.read(4)
    size = unpack('I',b)[0]

    data = f.read(size)

    return blocktype, data

def parse_datablock( data ):
    try:
        udata = unpack('d I I H 288H',data)
    except:
        return None

    value = udata[0]
    clock = udata[1]
    time = udata[2]
    temp = udata[3]
    header = [ udata[4:20], udata[148:164] ]
    data = [ udata[20:148], udata[164:] ]
    
    return value, clock, time, temp, header, data
    
#search for next data block, and return the channel values as a list of 2 128 channel tuples
def next_event( f ):
    btype = 10
    while btype != 2:
        try:
            (btype, data) = read_block(f)
        except:
            return None

    (value, clock, time, temp, header, data) = parse_datablock(data)
    return data



if __name__ == '__main__':
    for j in range(1,41):
        
        with open('/home/alibava-user/AlibavaRun/log%i.dat' % j) as f:
            (timestamp, runtype, info, pedestal, noise) = read_file_header(f)
            c = ROOT.TCanvas()
            h = ROOT.TH1F("noise","noise",len(noise),-0.5,len(noise)-0.5)
            for i,n in enumerate(noise[:128]):
                h.SetBinContent(i+1,n)
            h.Draw()
            c.SaveAs("plots/noise_%02i.png" % j)

            hits = ROOT.TH1F("hits","hits",len(noise),-0.5,len(noise)-0.5)
            sigs = ROOT.TH1F("sigs","sigs",512,-512,512)

            ev = next_event(f)
            while ev is not None:

                for i,raw in enumerate(ev[0]):
                    val = raw - pedestal[i]
                    
                    if val < 0 and abs(val) > 5*noise[i]  and abs(val) > 40:
                        print "Hit on channel",i
                        print "  Val =", val," noise =",noise[i]
                        hits.Fill( i )
                        sigs.Fill(val)
            
                ev = next_event(f)

            hits.Draw()
            c.SaveAs("plots/hits_%02i.png" % j)
            sigs.Draw()
            c.SaveAs("plots/sigs_%02i.png" % j)
