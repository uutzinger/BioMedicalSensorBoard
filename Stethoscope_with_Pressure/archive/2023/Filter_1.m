fc_l = 1150;    % Cutoff frequency for LPF
fc_h = 100;      % Cutoff frequency for HPF
fs = 8000;      % Sampling Frequency

% 2nd order LPF. To change the order of the filter, modify the first parameter in "butter(2,fc_l/(fs/2))"
[b_l,a_l] = butter(2,fc_l/(fs/2));
%freqz(b_l,a_l,[],fs);
%subplot(2,1,1);

% 2nd order HPF. To change the order of the filter, modify the first parameter in "butter(2,fc_l/(fs/2), 'high')"
[b_h,a_h] = butter(2,fc_h/(fs/2), 'high');
%freqz(b_h,a_h,[],fs);
%subplot(2,1,1);

% Multiply the transfer functions to create a bandpass filter
    % Create transfer function objects for LPF and HPF
    tf_lpf = tf(b_l, a_l, 1/fs) % LPF transfer function
    tf_hpf = tf(b_h, a_h, 1/fs) % HPF transfer function
    % Multiply the transfer functions to create a bandpass filter
    tf_bpf = tf_lpf * tf_hpf
    % Get the numerator and denominator coefficients of the bandpass filter
    [b_bpf, a_bpf] = tfdata(tf_bpf, 'v');

freqz(b_bpf,a_bpf,[],fs);
subplot(2,1,1);

% DC Gain 
dc_gain = sum(b_bpf) / sum(a_bpf);



