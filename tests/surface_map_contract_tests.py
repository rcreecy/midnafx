"""Parser fixtures use synthetic paths; no upstream patch is created or applied."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('contract', Path(__file__).with_name('surface_map_contract.py'))
contract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(contract)


class Contract(unittest.TestCase):
    files = {'fixture/cache.hpp': 'cache\n', 'fixture/kernel.hpp': 'kernel\n'}

    def payload(self):
        return ''.join('diff --git a/'+path+' b/'+path+'\nnew file mode 100644\n--- /dev/null\n+++ b/'+path+'\n@@ -0,0 +1 @@\n+'+body
                       for path, body in self.files.items())

    def test_absent_and_complete(self):
        self.assertFalse(contract.validate('', self.files))
        self.assertTrue(contract.validate(self.payload(), self.files))

    def test_partial_or_referenced_payload_fails(self):
        for patch in [self.payload().split('diff --git a/fixture/kernel.hpp')[0],
                      '#include "cache.hpp"', 'surface_maps::request()']:
            with self.assertRaises(ValueError): contract.validate(patch, self.files)

    def test_stale_and_duplicate_payloads_fail(self):
        for patch in [self.payload().replace('+kernel\n', '+stale\n'), self.payload()*2,
                      self.payload().replace('new file mode 100644', 'old file mode 100644')]:
            with self.assertRaises(ValueError): contract.validate(patch, self.files)


if __name__ == '__main__':
    unittest.main()
