#!/usr/bin/perl
use 5.16.0;
use warnings FATAL => 'all';
use POSIX ":sys_wait_h";

use Time::HiRes qw(time);
use Test::Simple tests => 14;

sub crc_check {
    my ($file, $expect) = @_;
    my $crc = `crc32 $file`;
    chomp $crc;
    unless ($crc eq $expect) {
        die "file modified: $file";
    }
}

crc_check("ivec_main.c", "cae7f5aa");
crc_check("list_main.c", "0ae6f792");
crc_check("frag_main.c", "d8d3af29");

sub get_time {
    my $data = `cat time.tmp | grep ^real`;
    $data =~ /^real\s+(.*)$/;
    return 0 + $1;
}

sub run_prog {
    my ($prog, $arg) = @_;
    system("rm -f outp.tmp time.tmp");
    system("timeout -k 30 20 time -p -o time.tmp ./$prog $arg > outp.tmp");
    return `cat outp.tmp`;
}

ok(-f "report.txt" && !-x "report.txt", "report.txt exists and isn't executable");
ok(-f "graph.png" && !-x "graph.png", "graph.png exists and isn't executable");

my $sys_v = run_prog("collatz-ivec-sys", 1000);
ok($sys_v =~ /at 871: 178 steps/, "ivec-sys 1k");

my $sys_l = run_prog("collatz-list-sys", 1000);
ok($sys_l =~ /at 871: 178 steps/, "list-sys 1k");

my $hw7_l = run_prog("collatz-list-hwx", 100);
ok($hw7_l =~ /at 97: 118 steps/, "list-hwx 100");

my $hw7_v = run_prog("collatz-ivec-hwx", 100);
ok($hw7_v =~ /at 97: 118 steps/, "ivec-hwx 100");

my $par_v = run_prog("collatz-ivec-opt", 1000);
my $pv_ok = $par_v =~ /at 871: 178 steps/;
ok($pv_ok, "ivec-par 1k");

my $par_l = run_prog("collatz-list-opt", 1000);
my $pl_ok = $par_l =~ /at 871: 178 steps/;
ok($pl_ok, "list-par 1k");

$par_v = run_prog("collatz-ivec-opt", 10000);
$pv_ok = $par_v =~ /at 6171: 261 steps/;
ok($pv_ok, "ivec-opt 10k");

$par_l = run_prog("collatz-list-opt", 10000);
$pl_ok = $par_l =~ /at 6171: 261 steps/;
ok($pl_ok, "list-opt 10k");

$par_v = run_prog("collatz-ivec-opt", 500000);
$pv_ok = $par_v =~ /at 410011: 448 steps/;
ok($pv_ok, "ivec-opt 500k");

$par_l = run_prog("collatz-list-opt", 500000);
$pl_ok = $par_l =~ /at 410011: 448 steps/;
ok($pl_ok, "list-opt 500k");

my $fragt = run_prog("frag-opt", 1);
my $ft_ok = $fragt =~ /frag test ok/;
ok($ft_ok, "fragmentation test");

sub clang_check {
    my $errs = `clang-check *.c -- 2>&1`;
    chomp $errs;
    if ($errs eq "") {
        return 1;
    }
    else {
        warn $errs;
        return 0;
    }
}

ok(clang_check(), "clang check");
