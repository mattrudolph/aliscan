#!/usr/bin/env python

from datareader import *
from fitfuncs import *

if __name__ == '__main__':
    indir = '/home/alibava-user/data/test/LaserScan/0047/'

    import sys
    sys.argv.append( '-b' )

    import os
    plotdir = indir + 'plots'
    if not os.path.exists(plotdir):
        os.makedirs(plotdir)
    
    #get the run files in that dir
    import re
    datfiles = [f for f in os.listdir(indir) if re.match("log[0-9]+\.dat",f) is not None]
    nfiles = len(datfiles)

    import ROOT
    c = ROOT.TCanvas()
    c.SetGridx()
    hped = ROOT.TH1F("pedestal","pedestal; Chan",128,-0.5,127.5)
    hnoise = ROOT.TH1F("noise","noise; Chan",128,-0.5,127.5)
    hits = ROOT.TH1F("hits","hits; Chan",64,63.5,127.5)
    sigs = ROOT.TH1F("sigs","sigs; ADC",512,-512,512)

    wronghits = ROOT.TH1F("wronghits","hits; Chan",64,63.5,127.5)
    wronghitscontrol = ROOT.TH1F("wronghitscontrol","hits; Chan",64,63.5,127.5)

    hitsvx = ROOT.TGraph()
    sigvx = ROOT.TGraph()
    hitsvy = ROOT.TGraph()
    sigvy = ROOT.TGraph()

    #one or 2 strip fit?
    myfitInst = myfit1gaus()
    myfunc = ROOT.TF1("myfunc",myfitInst,-250,0, 3)
    myfunc.SetParameter(0,1000)
    myfunc.SetParLimits(0,0,10000)
    myfunc.SetParameter(1,-200)
    myfunc.SetParLimits(1, -512,0)
    myfunc.SetParameter(2,10)
    myfunc.SetParLimits(2,0,100)

    myfitInst2 = myfit2gaus()
    myfunc2 = ROOT.TF1("myfunc",myfitInst,-250,0, 6)
    myfunc2.SetParameter(0,1000)
    myfunc2.SetParameter(1,-200)
    myfunc2.SetParameter(2,10)
    myfunc2.SetParameter(3,1000)
    myfunc2.SetParameter(4,-200)
    myfunc2.SetParameter(5,10)

    x0 = 0
    y0 = 0

    with open(indir+'console.out') as console:
        for j in range(1,nfiles+1):
        
            #find the iteration number
            line = console.readline()
            while re.match("### SCAN ITERATION: %i" % j, line) is None:
                line = console.readline()

            line = console.readline()
            [x,y] = [float(pos) for pos in re.findall("[0-9]+\.[0-9]",line)]
            if(j==1):
                x0 = x
                y0 = y


            with open(indir+('log%i.dat' % j)) as f:
                (timestamp, runtype, info, pedestal, noise) = read_file_header(f)

                if(j == 1):
                    for i,n in enumerate(pedestal[:128]):
                        hped.SetBinContent(i+1,n)
                    hped.Draw()
                    c.SaveAs(plotdir+"/pedestal.png")

            
                    for i,n in enumerate(noise[:128]):
                        hnoise.SetBinContent(i+1,n)
                    hnoise.Draw()
                    c.SaveAs(plotdir+"/noise.png")

                    print "n82=",noise[82],"; n83=",noise[83]

                hits.Reset()
                sigs.Reset()

                ev = next_event(f)
                while ev is not None:

                    for i,raw in enumerate(ev[0]):
                        if( i < 64):
                            continue
                        val = raw - pedestal[i]
                    
                        if val < 0 and abs(val) > 5*noise[i]:
                            #print "Hit on channel",i
                            #print "  Val =", val," noise =",noise[i]
                            if( i==82):
                                hits.Fill( i )
                                sigs.Fill(val)

                            if i < 82:
                                if (x > 500 and x<700) or (x>1300 and x<1500):
                                    wronghits.Fill(i)
                                elif ( x>200 and x<400) or (x>800 and x<1000):
                                    wronghitscontrol.Fill(i)
            
                    ev = next_event(f)

                #most common hit
                hitchan = hits.GetMaximumBin()
                hitsvx.SetPoint(hitsvx.GetN(), x0-x, hits.GetBinContent(hitchan))
                hitsvy.SetPoint(hitsvx.GetN(), y0-y, hits.GetBinContent(hitchan))
                    
                hits.Draw()
                c.SaveAs(plotdir+("/hits_%04i.png" % j))

                myfitInst.thr = 7*noise[hitchan-1] 
                myfunc.SetParameter(0,4000)
                myfunc.SetParameter(1,-200)
                myfunc.SetParameter(2,20)
                res = sigs.Fit(myfunc,"S","",-512,-50)
                status = res.Status()
                print status
                peak = abs(myfunc.GetParameter(1))
                if(status ==0):
                    sigvx.SetPoint(sigvx.GetN(), x0-x, peak)
                    sigvy.SetPoint(sigvy.GetN(), y0-y, peak)
                sigs.Draw()
                c.SaveAs(plotdir+("/sigs_%04i.png" % j))


    hitsvx.SetMarkerStyle(20)
    hitsvy.SetMarkerStyle(20)
    sigvx.SetMarkerStyle(20)
    sigvy.SetMarkerStyle(20)

    hitsvx.Draw("AP")
    c.SaveAs(plotdir+"/hitsvx.png")
    sigvx.Draw("AP")
    sigvx.GetYaxis().SetRangeUser(0,400)
    c.SaveAs(plotdir+"/sigvx.png")
    hitsvy.Draw("AP")
    c.SaveAs(plotdir+"/hitsvy.png")
    sigvy.Draw("AP")
    sigvy.GetYaxis().SetRangeUser(0,400)
    c.SaveAs(plotdir+"/sigvy.png")

    wronghits.SetLineColor(ROOT.kRed)
    wronghitscontrol.SetLineColor(ROOT.kBlue)
    leg = ROOT.TLegend(0.7,0.7,0.9,0.9)
    leg.AddEntry(wronghits,"Trace region","L")
    leg.AddEntry(wronghitscontrol,"Control region","L")
    wronghits.Draw("HIST")
    wronghitscontrol.Draw("HIST SAME")
    c.SaveAs(plotdir+"/wronghits.png")
