#!/usr/bin/env python


from datareader import *

from fitfuncs import *


if __name__ == '__main__':
    indir = '/home/alibava-user/data/test/LaserScan/0004/'
    num = 41

    import sys
    sys.argv.append( '-b' )

    import ROOT
    c = ROOT.TCanvas()
    hits = ROOT.TH1F("hits","hits",128,-0.5,127.5)
    sigs = ROOT.TH1F("sigs","sigs",512,-512,512)

    gr0 = ROOT.TGraph()
    gr1 = ROOT.TGraph()

    peak0 = []
    peak1 = []

    myfitInst = myfit2gaus()

    myfunc = ROOT.TF1("myfunc",myfitInst,-250,0, 6)
    myfunc.SetParameter(0,1000)
    myfunc.SetParameter(1,-200)
    myfunc.SetParameter(2,10)
    myfunc.SetParameter(3,1000)
    myfunc.SetParameter(4,-20)
    myfunc.SetParameter(5,10)


    for j in range(1,num+1):

        with open(indir+('log%i.dat' % j)) as f:
            (timestamp, runtype, info, pedestal, noise) = read_file_header(f)

            hits.Reset()
            sigs.Reset()

            ev = next_event(f)

            while ev is not None:

                for i,raw in enumerate(ev[0]):
                    #Set this to the strips you are using for the profile
                    if i!= 88 and i!=89:
                        continue
                    val = raw - pedestal[i]
                    
                    if val < 0 and abs(val) > 5*noise[i]:

                        hits.Fill( i )
                        sigs.Fill(val)

                ev = next_event(f)

            #Now try to find the two peaks

            sigs.Draw()
            sigs.Fit("myfunc")
            sigs.Draw()
            c.SaveAs(indir+("plots/sigs_%02i.png" % j))

            if ( j < 22 ):
                gr0.SetPoint(gr0.GetN(), j-21.5, -myfunc.GetParameter(1))
                gr1.SetPoint(gr1.GetN(), j-21.5, -myfunc.GetParameter(4))
                peak0 += [-myfunc.GetParameter(1),]
                peak1 += [-myfunc.GetParameter(4),]
            else:
                gr0.SetPoint(gr0.GetN(), j-21.5, -myfunc.GetParameter(4))
                gr1.SetPoint(gr1.GetN(), j-21.5, -myfunc.GetParameter(1))
                peak0 += [-myfunc.GetParameter(4),]
                peak1 += [-myfunc.GetParameter(1),]


    gr0.SetMarkerStyle(20)
    gr1.SetMarkerStyle(20)
    gr0.SetLineColor(ROOT.kBlue)
    gr0.SetMarkerColor(ROOT.kBlue)
    gr1.SetLineColor(ROOT.kRed)
    gr1.SetMarkerColor(ROOT.kRed)
    
    gr0.Draw("AP")
    gr0.GetXaxis().SetTitle("y [#mum]")
    gr0.GetYaxis().SetTitle("Peak ADC")
    gr0.GetYaxis().SetRangeUser(0,180)
    gr1.Draw("SAME P")

    leg = ROOT.TLegend(0.7,0.4,0.9,0.6)
    leg.AddEntry(gr0,"Strip 89","P")
    leg.AddEntry(gr1,"Strip 88","P")
    leg.Draw()


    p1 = ROOT.TF1("p1","[0] + [1]*x",-5,5)

    gr0.Fit(p1,"","",-5,5)
    slope0 = p1.GetParameter(1)
    gr1.Fit(p1,"","",-5,5)
    slope1 = p1.GetParameter(1)

    print "Avg slope = ", (slope1-slope0)/2

    c.SaveAs(indir+"plots/peak_pos.png")

    grprof = ROOT.TGraph()
    for i,f in enumerate(peak0):
        if i==0 or i==40:
            continue
        
        d0 = - ( peak0[i+1] - peak0[i-1])/2
        d1 = (peak1[i+1] - peak1[i-1])/2
        if(i<21):
            grprof.SetPoint( grprof.GetN(), i-20.5, d0)
        else:
            grprof.SetPoint( grprof.GetN(), i-20.5, d1)

#        grprof.SetPoint( grprof.GetN(), i-20.5, (d0 + d1)/2)

    grprof.SetMarkerStyle(20)

    grprof.Draw("AP")
    grprof.GetXaxis().SetTitle("y [#mum]")
    grprof.GetYaxis().SetTitle("dADC/dy")

    c.SaveAs(indir+"plots/beamprofile.png")
