#pragma once
// ---------------------------------------------------------------------------
// "ToTape9" - porting dell'algoritmo di Airwindows (Chris Johnson), licenza MIT.
// Vedi THIRD_PARTY_NOTICES.txt. Stesso ordine e stessi calcoli dell'originale
// (Dubly encode, flutter, bias, isteresi, TapeHack/TapeHack2, head bump,
// Dubly decode, ClipOnly3). Differenza: niente dither (si lavora in float).
// ---------------------------------------------------------------------------
#include <cmath>
#include <cstdint>
#include <algorithm>

class AirwindowsToTape9
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        overallscale = sr / 44100.0;
        spacing = (int) std::floor (overallscale); spacing = std::clamp (spacing, 1, 16);
        slewsing = (int) std::floor (overallscale * 2.0); slewsing = std::clamp (slewsing, 2, 32);
        iirEncL = iirDecL = iirEncR = iirDecR = 0.0;
        compEncL = compDecL = compEncR = compDecR = 1.0;
        avgEncL = avgDecL = avgEncR = avgDecR = 0.0;
        for (int i = 0; i < 1002; ++i) { dL[i] = 0.0; dR[i] = 0.0; }
        sweepL = sweepR = pi; nextmaxL = nextmaxR = 0.5; gcount = 0;
        for (auto& v : gslew) v = 0.0;
        hysteresisL = hysteresisR = 0.0; headBumpL = headBumpR = 0.0;
        for (int i = 0; i < hdb_total; ++i) { hdbA[i] = 0.0; hdbB[i] = 0.0; }
        for (int i = 0; i < 33; ++i) { avg32L[i] = post32L[i] = avg32R[i] = post32R[i] = 0.0; }
        for (int i = 0; i < 17; ++i) { avg16L[i] = post16L[i] = avg16R[i] = post16R[i] = 0.0; }
        for (int i = 0; i < 9; ++i)  { avg8L[i] = post8L[i] = avg8R[i] = post8R[i] = 0.0; }
        for (int i = 0; i < 5; ++i)  { avg4L[i] = post4L[i] = avg4R[i] = post4R[i] = 0.0; }
        for (int i = 0; i < 3; ++i)  { avg2L[i] = post2L[i] = avg2R[i] = post2R[i] = 0.0; }
        avgPos = 0; lastDarkL = lastDarkR = 0.0;
        lastSampleL = lastSampleR = 0.0; wasPosClipL = wasNegClipL = wasPosClipR = wasNegClipR = false;
        for (int i = 0; i < 18; ++i) { intermediateL[i] = intermediateR[i] = 0.0; }
        for (int i = 0; i < 34; ++i) { slewL[i] = slewR[i] = 0.0; }
        fpdL = 2756923396u; fpdR = 3466190218u;
    }

    int latencySamples() const { return spacing; }

    // A Input, B Tilt, C Shape, D Flutter, E FlutSpd, F Bias, G HeadBmp, H HeadFrq, I Output (tutti 0-1)
    void setParams (double A, double B, double C, double D, double E, double F, double G, double H, double I)
    {
        inputGain = std::pow (A * 2.0, 2.0);
        dublyAmount = B * 2.0;
        outlyAmount = (1.0 - B) * -2.0; if (outlyAmount < -1.0) outlyAmount = -1.0;
        iirEncFreq = (1.0 - C) / overallscale;
        iirDecFreq = C / overallscale;
        flutDepth = std::pow (D, 6) * overallscale * 50; if (flutDepth > 498.0) flutDepth = 498.0;
        flutFrequency = (0.02 * std::pow (E, 3)) / overallscale;
        bias = (F * 2.0) - 1.0;
        underBias = (std::pow (bias, 4) * 0.25) / overallscale;
        double overBias = std::pow (1.0 - bias, 3) / overallscale;
        if (bias > 0.0) underBias = 0.0;
        if (bias < 0.0) overBias = 1.0 / overallscale;
        const int th[9] = { threshold9, threshold8, threshold7, threshold6, threshold5, threshold4, threshold3, threshold2, threshold1 };
        for (int k = 0; k < 9; ++k) { gslew[th[k]] = overBias; overBias *= 1.618033988749894848204586; }

        headBumpDrive = (G * 0.1) / overallscale;
        headBumpMix = G * 0.5;
        hdbA[hdb_freq] = (((H * H) * 175.0) + 25.0) / sr;
        hdbB[hdb_freq] = hdbA[hdb_freq] * 0.9375;
        hdbB[hdb_reso] = hdbA[hdb_reso] = 0.618033988749894848204586;
        hdbB[hdb_a1] = hdbA[hdb_a1] = 0.0;
        double K = std::tan (pi * hdbA[hdb_freq]);
        double norm = 1.0 / (1.0 + K / hdbA[hdb_reso] + K * K);
        hdbA[hdb_a0] = K / hdbA[hdb_reso] * norm; hdbA[hdb_a2] = -hdbA[hdb_a0];
        hdbA[hdb_b1] = 2.0 * (K * K - 1.0) * norm; hdbA[hdb_b2] = (1.0 - K / hdbA[hdb_reso] + K * K) * norm;
        K = std::tan (pi * hdbB[hdb_freq]);
        norm = 1.0 / (1.0 + K / hdbB[hdb_reso] + K * K);
        hdbB[hdb_a0] = K / hdbB[hdb_reso] * norm; hdbB[hdb_a2] = -hdbB[hdb_a0];
        hdbB[hdb_b1] = 2.0 * (K * K - 1.0) * norm; hdbB[hdb_b2] = (1.0 - K / hdbB[hdb_reso] + K * K) * norm;
        outputGain = I * 2.0;
    }

    void process (double& inL, double& inR)
    {
        double inputSampleL = inL, inputSampleR = inR;
        if (std::fabs (inputSampleL) < 1.18e-23) inputSampleL = fpdL * 1.18e-17;
        if (std::fabs (inputSampleR) < 1.18e-23) inputSampleR = fpdR * 1.18e-17;
        if (inputGain != 1.0) { inputSampleL *= inputGain; inputSampleR *= inputGain; }

        // Dubly encode
        dublyEncode (inputSampleL, iirEncL, avgEncL, compEncL);
        dublyEncode (inputSampleR, iirEncR, avgEncR, compEncR);

        // Flutter
        if (flutDepth > 0.0)
        {
            if (gcount < 0 || gcount > 999) gcount = 999;
            dL[gcount] = inputSampleL;
            int count = gcount;
            double offset = flutDepth + (flutDepth * std::sin (sweepL));
            sweepL += nextmaxL * flutFrequency;
            if (sweepL > (pi * 2.0))
            {
                sweepL -= pi * 2.0;
                double flutA = 0.24 + (fpdL / (double) UINT32_MAX * 0.74);
                fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
                double flutB = 0.24 + (fpdL / (double) UINT32_MAX * 0.74);
                if (std::fabs (flutA - std::sin (sweepR + nextmaxR)) < std::fabs (flutB - std::sin (sweepR + nextmaxR))) nextmaxL = flutA; else nextmaxL = flutB;
            }
            count += (int) std::floor (offset);
            inputSampleL = (dL[count - ((count > 999) ? 1000 : 0)] * (1 - (offset - std::floor (offset))));
            inputSampleL += (dL[count + 1 - ((count + 1 > 999) ? 1000 : 0)] * (offset - std::floor (offset)));
            dR[gcount] = inputSampleR;
            count = gcount;
            offset = flutDepth + (flutDepth * std::sin (sweepR));
            sweepR += nextmaxR * flutFrequency;
            if (sweepR > (pi * 2.0))
            {
                sweepR -= pi * 2.0;
                double flutA = 0.24 + (fpdR / (double) UINT32_MAX * 0.74);
                fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
                double flutB = 0.24 + (fpdR / (double) UINT32_MAX * 0.74);
                if (std::fabs (flutA - std::sin (sweepL + nextmaxL)) < std::fabs (flutB - std::sin (sweepL + nextmaxL))) nextmaxR = flutA; else nextmaxR = flutB;
            }
            count += (int) std::floor (offset);
            inputSampleR = (dR[count - ((count > 999) ? 1000 : 0)] * (1 - (offset - std::floor (offset))));
            inputSampleR += (dR[count + 1 - ((count + 1 > 999) ? 1000 : 0)] * (offset - std::floor (offset)));
            gcount--;
        }

        // Bias
        if (std::fabs (bias) > 0.001)
        {
            for (int x = 0; x < gslew_total; x += 3)
            {
                if (underBias > 0.0)
                {
                    double stuck = std::fabs (inputSampleL - (gslew[x] / 0.975)) / underBias;
                    if (stuck < 1.0) inputSampleL = (inputSampleL * stuck) + ((gslew[x] / 0.975) * (1.0 - stuck));
                    stuck = std::fabs (inputSampleR - (gslew[x + 1] / 0.975)) / underBias;
                    if (stuck < 1.0) inputSampleR = (inputSampleR * stuck) + ((gslew[x + 1] / 0.975) * (1.0 - stuck));
                }
                if ((inputSampleL - gslew[x]) > gslew[x + 2]) inputSampleL = gslew[x] + gslew[x + 2];
                if (-(inputSampleL - gslew[x]) > gslew[x + 2]) inputSampleL = gslew[x] - gslew[x + 2];
                gslew[x] = inputSampleL * 0.975;
                if ((inputSampleR - gslew[x + 1]) > gslew[x + 2]) inputSampleR = gslew[x + 1] + gslew[x + 2];
                if (-(inputSampleR - gslew[x + 1]) > gslew[x + 2]) inputSampleR = gslew[x + 1] - gslew[x + 2];
                gslew[x + 1] = inputSampleR * 0.975;
            }
        }

        // Isteresi
        double applyHysteresis = (1.0 - std::fabs (inputSampleL)) * (1.0 - std::fabs (inputSampleL)) * 0.012;
        hysteresisL = std::fmax (std::fmin (hysteresisL + ((inputSampleL * std::fabs (inputSampleL))), 0.011449), -0.011449) * 0.999;
        inputSampleL += (hysteresisL * applyHysteresis);
        applyHysteresis = (1.0 - std::fabs (inputSampleR)) * (1.0 - std::fabs (inputSampleR)) * 0.012;
        hysteresisR = std::fmax (std::fmin (hysteresisR + ((inputSampleR * std::fabs (inputSampleR))), 0.011449), -0.011449) * 0.999;
        inputSampleR += (hysteresisR * applyHysteresis);

        // TapeHack2 (prima della saturazione)
        double darkSampleL = inputSampleL, darkSampleR = inputSampleR;
        if (avgPos > 31) avgPos = 0;
        average (darkSampleL, darkSampleR, avg32L, avg32R, avg16L, avg16R, avg8L, avg8R, avg4L, avg4R, avg2L, avg2R);
        double avgSlewL = std::fmin (std::fabs (lastDarkL - inputSampleL) * 0.12 * overallscale, 1.0);
        avgSlewL = 1.0 - (1.0 - avgSlewL * 1.0 - avgSlewL);
        inputSampleL = (inputSampleL * (1.0 - avgSlewL)) + (darkSampleL * avgSlewL);
        lastDarkL = darkSampleL;
        double avgSlewR = std::fmin (std::fabs (lastDarkR - inputSampleR) * 0.12 * overallscale, 1.0);
        avgSlewR = 1.0 - (1.0 - avgSlewR * 1.0 - avgSlewR);
        inputSampleR = (inputSampleR * (1.0 - avgSlewR)) + (darkSampleR * avgSlewR);
        lastDarkR = darkSampleR;

        // TapeHack
        inputSampleL = tapeHack (inputSampleL);
        inputSampleR = tapeHack (inputSampleR);

        darkSampleL = inputSampleL; darkSampleR = inputSampleR;
        if (avgPos > 31) avgPos = 0;
        average (darkSampleL, darkSampleR, post32L, post32R, post16L, post16R, post8L, post8R, post4L, post4R, post2L, post2R);
        avgPos++;
        inputSampleL = (inputSampleL * (1.0 - avgSlewL)) + (darkSampleL * avgSlewL);
        inputSampleR = (inputSampleR * (1.0 - avgSlewR)) + (darkSampleR * avgSlewR);

        // HeadBump
        double headBumpSampleL = 0.0, headBumpSampleR = 0.0;
        if (headBumpMix > 0.0)
        {
            headBumpL += (inputSampleL * headBumpDrive);
            headBumpL -= (headBumpL * headBumpL * headBumpL * (0.0618 / std::sqrt (overallscale)));
            headBumpR += (inputSampleR * headBumpDrive);
            headBumpR -= (headBumpR * headBumpR * headBumpR * (0.0618 / std::sqrt (overallscale)));
            double headBiqSampleL = (headBumpL * hdbA[hdb_a0]) + hdbA[hdb_sL1];
            hdbA[hdb_sL1] = (headBumpL * hdbA[hdb_a1]) - (headBiqSampleL * hdbA[hdb_b1]) + hdbA[hdb_sL2];
            hdbA[hdb_sL2] = (headBumpL * hdbA[hdb_a2]) - (headBiqSampleL * hdbA[hdb_b2]);
            headBumpSampleL = (headBiqSampleL * hdbB[hdb_a0]) + hdbB[hdb_sL1];
            hdbB[hdb_sL1] = (headBiqSampleL * hdbB[hdb_a1]) - (headBumpSampleL * hdbB[hdb_b1]) + hdbB[hdb_sL2];
            hdbB[hdb_sL2] = (headBiqSampleL * hdbB[hdb_a2]) - (headBumpSampleL * hdbB[hdb_b2]);
            double headBiqSampleR = (headBumpR * hdbA[hdb_a0]) + hdbA[hdb_sR1];
            hdbA[hdb_sR1] = (headBumpR * hdbA[hdb_a1]) - (headBiqSampleR * hdbA[hdb_b1]) + hdbA[hdb_sR2];
            hdbA[hdb_sR2] = (headBumpR * hdbA[hdb_a2]) - (headBiqSampleR * hdbA[hdb_b2]);
            headBumpSampleR = (headBiqSampleR * hdbB[hdb_a0]) + hdbB[hdb_sR1];
            hdbB[hdb_sR1] = (headBiqSampleR * hdbB[hdb_a1]) - (headBumpSampleR * hdbB[hdb_b1]) + hdbB[hdb_sR2];
            hdbB[hdb_sR2] = (headBiqSampleR * hdbB[hdb_a2]) - (headBumpSampleR * hdbB[hdb_b2]);
        }
        inputSampleL += (headBumpSampleL * headBumpMix);
        inputSampleR += (headBumpSampleR * headBumpMix);

        // Dubly decode
        dublyDecode (inputSampleL, iirDecL, avgDecL, compDecL);
        dublyDecode (inputSampleR, iirDecR, avgDecR, compDecR);

        if (outputGain != 1.0) { inputSampleL *= outputGain; inputSampleR *= outputGain; }

        // ClipOnly3
        clipOnly3 (inputSampleL, fpdL, lastSampleL, wasPosClipL, wasNegClipL, slewL, intermediateL);
        clipOnly3 (inputSampleR, fpdR, lastSampleR, wasPosClipR, wasNegClipR, slewR, intermediateR);

        // aggiornamento del generatore (nell'originale lo fa il dither)
        fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
        fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;

        inL = inputSampleL; inR = inputSampleR;
    }

private:
    static constexpr double pi = 3.14159265358979323846;

    void dublyEncode (double& s, double& iir, double& avg, double& comp)
    {
        iir = (iir * (1.0 - iirEncFreq)) + (s * iirEncFreq);
        double highPart = ((s - iir) * 2.848);
        highPart += avg; avg = (s - iir) * 1.152;
        if (highPart > 1.0) highPart = 1.0; if (highPart < -1.0) highPart = -1.0;
        double dubly = std::fabs (highPart);
        if (dubly > 0.0)
        {
            double adjust = std::log (1.0 + (255.0 * dubly)) / 2.40823996531;
            if (adjust > 0.0) dubly /= adjust;
            comp = (comp * (1.0 - iirEncFreq)) + (dubly * iirEncFreq);
            s += ((highPart * comp) * dublyAmount);
        }
    }
    void dublyDecode (double& s, double& iir, double& avg, double& comp)
    {
        iir = (iir * (1.0 - iirDecFreq)) + (s * iirDecFreq);
        double highPart = ((s - iir) * 2.628);
        highPart += avg; avg = (s - iir) * 1.372;
        if (highPart > 1.0) highPart = 1.0; if (highPart < -1.0) highPart = -1.0;
        double dubly = std::fabs (highPart);
        if (dubly > 0.0)
        {
            double adjust = std::log (1.0 + (255.0 * dubly)) / 2.40823996531;
            if (adjust > 0.0) dubly /= adjust;
            comp = (comp * (1.0 - iirDecFreq)) + (dubly * iirDecFreq);
            s += ((highPart * comp) * outlyAmount);
        }
    }
    void average (double& l, double& r, double* a32L, double* a32R, double* a16L, double* a16R, double* a8L, double* a8R,
                  double* a4L, double* a4R, double* a2L, double* a2R)
    {
        if (slewsing > 31) { a32L[avgPos] = l; a32R[avgPos] = r; l = r = 0.0; for (int x = 0; x < 32; x++) { l += a32L[x]; r += a32R[x]; } l /= 32.0; r /= 32.0; }
        if (slewsing > 15) { a16L[avgPos % 16] = l; a16R[avgPos % 16] = r; l = r = 0.0; for (int x = 0; x < 16; x++) { l += a16L[x]; r += a16R[x]; } l /= 16.0; r /= 16.0; }
        if (slewsing > 7)  { a8L[avgPos % 8] = l; a8R[avgPos % 8] = r; l = r = 0.0; for (int x = 0; x < 8; x++) { l += a8L[x]; r += a8R[x]; } l /= 8.0; r /= 8.0; }
        if (slewsing > 3)  { a4L[avgPos % 4] = l; a4R[avgPos % 4] = r; l = r = 0.0; for (int x = 0; x < 4; x++) { l += a4L[x]; r += a4R[x]; } l /= 4.0; r /= 4.0; }
        if (slewsing > 1)  { a2L[avgPos % 2] = l; a2R[avgPos % 2] = r; l = r = 0.0; for (int x = 0; x < 2; x++) { l += a2L[x]; r += a2R[x]; } l /= 2.0; r /= 2.0; }
    }
    static double tapeHack (double s)
    {
        s = std::fmax (std::fmin (s, 2.305929007734908), -2.305929007734908);
        double addtwo = s * s;
        double empower = s * addtwo;
        s -= (empower / 6.0);
        empower *= addtwo; s += (empower / 69.0);
        empower *= addtwo; s -= (empower / 2530.08);
        empower *= addtwo; s += (empower / 224985.6);
        empower *= addtwo; s -= (empower / 9979200.0f);
        return s;
    }
    void clipOnly3 (double& s, uint32_t fpd, double& lastSample, bool& wasPos, bool& wasNeg, double* slew, double* intermediate)
    {
        double noise = 1.0 - ((double (fpd) / UINT32_MAX) * 0.076);
        if (wasPos) { if (s < lastSample) lastSample = (0.9085097 * noise) + (s * (1.0 - noise)); else lastSample = 0.94; }
        wasPos = false;
        if (s > 0.9085097) { wasPos = true; s = (0.9085097 * noise) + (lastSample * (1.0 - noise)); }
        if (wasNeg) { if (s > lastSample) lastSample = (-0.9085097 * noise) + (s * (1.0 - noise)); else lastSample = -0.94; }
        wasNeg = false;
        if (s < -0.9085097) { wasNeg = true; s = (-0.9085097 * noise) + (lastSample * (1.0 - noise)); }
        slew[spacing * 2] = std::fabs (lastSample - s);
        for (int x = spacing * 2; x > 0; x--) slew[x - 1] = slew[x];
        intermediate[spacing] = s; s = lastSample;
        for (int x = spacing; x > 0; x--) { intermediate[x - 1] = intermediate[x]; }
        lastSample = intermediate[0];
        if (wasPos || wasNeg) { for (int x = spacing; x > 0; x--) lastSample += intermediate[x]; lastSample /= spacing; }
        double finalSlew = 0.0;
        for (int x = spacing * 2; x >= 0; x--) if (finalSlew < slew[x]) finalSlew = slew[x];
        double postclip = 0.94 / (1.0 + (finalSlew * 1.3986013));
        if (s > postclip) s = postclip; if (s < -postclip) s = -postclip;
    }

    enum { prevSampL1, prevSampR1, threshold1, prevSampL2, prevSampR2, threshold2, prevSampL3, prevSampR3, threshold3,
           prevSampL4, prevSampR4, threshold4, prevSampL5, prevSampR5, threshold5, prevSampL6, prevSampR6, threshold6,
           prevSampL7, prevSampR7, threshold7, prevSampL8, prevSampR8, threshold8, prevSampL9, prevSampR9, threshold9, gslew_total };
    enum { hdb_freq, hdb_reso, hdb_a0, hdb_a1, hdb_a2, hdb_b1, hdb_b2, hdb_sL1, hdb_sL2, hdb_sR1, hdb_sR2, hdb_total };

    double sr = 44100.0, overallscale = 1.0;
    int spacing = 1, slewsing = 2;
    double inputGain = 1.0, dublyAmount = 1.0, outlyAmount = -1.0, iirEncFreq = 0.5, iirDecFreq = 0.5;
    double flutDepth = 0.0, flutFrequency = 0.0, bias = 0.0, underBias = 0.0;
    double headBumpDrive = 0.0, headBumpMix = 0.0, outputGain = 1.0;

    double iirEncL = 0, iirDecL = 0, compEncL = 1, compDecL = 1, avgEncL = 0, avgDecL = 0;
    double iirEncR = 0, iirDecR = 0, compEncR = 1, compDecR = 1, avgEncR = 0, avgDecR = 0;
    double dL[1002] {}, dR[1002] {};
    double sweepL = pi, sweepR = pi, nextmaxL = 0.5, nextmaxR = 0.5;
    int gcount = 0;
    double gslew[gslew_total] {};
    double hysteresisL = 0, hysteresisR = 0, headBumpL = 0, headBumpR = 0;
    double hdbA[hdb_total] {}, hdbB[hdb_total] {};
    double avg32L[33] {}, avg32R[33] {}, avg16L[17] {}, avg16R[17] {}, avg8L[9] {}, avg8R[9] {}, avg4L[5] {}, avg4R[5] {}, avg2L[3] {}, avg2R[3] {};
    double post32L[33] {}, post32R[33] {}, post16L[17] {}, post16R[17] {}, post8L[9] {}, post8R[9] {}, post4L[5] {}, post4R[5] {}, post2L[3] {}, post2R[3] {};
    double lastDarkL = 0, lastDarkR = 0;
    int avgPos = 0;
    double lastSampleL = 0, lastSampleR = 0, intermediateL[18] {}, intermediateR[18] {}, slewL[34] {}, slewR[34] {};
    bool wasPosClipL = false, wasNegClipL = false, wasPosClipR = false, wasNegClipR = false;
    uint32_t fpdL = 2756923396u, fpdR = 3466190218u;
};
