module Main where

import Prelude
import Control.Monad.Eff (Eff)
import Control.Monad.Eff.Console (CONSOLE, log, logShow)
import Data.Either (Either(..))
import Data.Tuple (Tuple)

foreign import data PCREOption :: *
foreign import data PCRECode :: *

foreign import caseless :: PCREOption
foreign import dotall   :: PCREOption

foreign import compile :: String -> Array PCREOption -> Either String PCRECode

foreign import capturedCount :: PCRECode -> Int

foreign import exec :: PCRECode ->
                       String ->
                       Int ->
                       Array PCREOption ->
                       Int ->
                       Either Int (Array (Tuple Int Int))

main :: forall e. Eff ( console :: CONSOLE | e ) Unit
main = do
  case (compile "^([^!]+)!(.+)=apquxz\\.ixr\\.zzz\\.ac\\.uk$" [caseless]) of
    Left err -> log err
    Right pcre -> do
      let sz = (capturedCount pcre + 1) * 3
          result = exec pcre "abc!pqr=apquxz.ixr.zzz.ac.uk" 0 [] sz
      logShow result
