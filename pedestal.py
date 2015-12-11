#!/usr/bin/env python


from datareader import *



if __name__ == '__main__':
    indir = '/home/alibava-user/data/test/LaserScan/0022/'


    import sys
    sys.argv.append( '-b' )

    import os
    plotdir = indir + 'pedplots'
    if not os.path.exists(plotdir):
        os.makedirs(plotdir)

    import ROOT
    c = ROOT.TCanvas()
    tped = ROOT.TH1F("theirpedestal","pedestal",128,-0.5,127.5)
    tnoise = ROOT.TH1F("theirnoise","noise",128,-0.5,127.5)

    myped = ROOT.TProfile("mypedestal","pedestal",128,-0.5,127.5)
    mypedsp = ROOT.TProfile("mypedestalsp","pedestal",128,-0.5,127.5,'S')
    #mynoise = ROOT.TProfile("mynoise","noise",128,-0.5,127.5,'S')
    mynoise = ROOT.TH1F("mynoise","noise",128,-0.5,127.5)

    with open(indir+'pedestal.dat') as f:
        (timestamp, runtype, info, pedestal, noise) = read_file_header(f)
        for i,n in enumerate(pedestal[:128]):
            tped.SetBinContent(i+1,n)
        tped.Draw()
        c.SaveAs(plotdir+"/theirpedestal.png")
    
        for i,n in enumerate(noise[:128]):
            tnoise.SetBinContent(i+1,n)
        tnoise.Draw()
        c.SaveAs(plotdir+"/theirnoise.png")

        ev = next_event(f)

        while ev is not None:

            for i,raw in enumerate(ev[0]):
                myped.Fill(i,raw)
                mypedsp.Fill(i,raw)
                
            ev = next_event(f)

    for i in range(1,129):
        mynoise.SetBinContent(i, mypedsp.GetBinError(i) )

    myped.Draw()
    c.SaveAs(plotdir+"/mypedestal.png")

    mynoise.Draw()
    c.SaveAs(plotdir+"/mynoise.png")
