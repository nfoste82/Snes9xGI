import importlib.util
from pathlib import Path
import unittest

spec=importlib.util.spec_from_file_location('critical',Path(__file__).with_name('run_lighting_critical_path.py'))
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class CriticalPathSummaryTest(unittest.TestCase):
    def test_preserve_outliers(self):
        result=module.stats([1]*99+[120])
        self.assertEqual(result,dict(n=100,p50=1,p95=1,p99=1,max=120))

    def test_no_percentile_addition(self):
        cpu=[1,100];gpu=[100,1]
        self.assertEqual(module.stats([c+g for c,g in zip(cpu,gpu)])['max'],101)
        self.assertNotEqual(module.stats(cpu)['max']+module.stats(gpu)['max'],101)

if __name__=='__main__':unittest.main()
